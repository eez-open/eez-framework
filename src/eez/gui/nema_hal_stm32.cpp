/*
 * nema_hal_stm32.c — NemaGFX Hardware Abstraction Layer for STM32H7RS
 *
 * NemaGFX is Think Silicon's GPU library. It is designed to be portable: all
 * platform-specific operations (memory allocation, register access, cache
 * management, synchronisation) are abstracted behind a set of functions whose
 * signatures are declared in <nema_hal.h>.  This file provides the concrete
 * implementations of those functions for the STM32H7RS SoC that carries the
 * NemaGFX-compatible GPU2D peripheral.
 *
 * Architecture overview
 * ─────────────────────
 *  ┌─────────────────────────────────────────────────────┐
 *  │  Application / EEZ GUI layer                        │
 *  │  calls nema_*() API from NemaGFX SDK                │
 *  └────────────────────┬────────────────────────────────┘
 *                       │ uses
 *  ┌────────────────────▼────────────────────────────────┐
 *  │  NemaGFX SDK (nema_core, nema_rasterizer …)         │
 *  │  calls nema_hal_* / nema_buffer_* / nema_reg_*      │
 *  └────────────────────┬────────────────────────────────┘
 *                       │ implemented here ↓
 *  ┌────────────────────▼────────────────────────────────┐
 *  │  THIS FILE  (nema_hal_stm32.c)                      │
 *  │  • memory pool via tsi_malloc                       │
 *  │  • register I/O via STM32 HAL GPU2D driver          │
 *  │  • command-list ring buffer                         │
 *  └────────────────────┬────────────────────────────────┘
 *                       │
 *  ┌────────────────────▼────────────────────────────────┐
 *  │  STM32 HAL  (gpu2d.h / HAL_GPU2D_*)                 │
 *  │  Hardware GPU2D peripheral registers                │
 *  └─────────────────────────────────────────────────────┘
 *
 * Memory model
 * ────────────
 * NemaGFX needs a contiguous, physically-addressed memory region it can pass
 * to the GPU as DMA targets.  On this platform that region lives in external
 * RAM (EXTRAM) which is directly accessible by the GPU2D's DMA engine.
 *
 * tsi_malloc is Think Silicon's pool allocator that manages that region.
 * Pool 0 (s_nemaPool0) holds all GPU-visible allocations: command-list ring
 * buffer descriptors, texture data, intermediate render targets, etc.
 *
 * Command-list ring buffer
 * ────────────────────────
 * NemaGFX records GPU commands into command lists (nema_cmdlist_t).  Between
 * the CPU writing commands and the GPU consuming them there is a ring buffer
 * (s_nemaRing) that acts as a FIFO.  Each submitted command list gets an
 * integer ID (clId); the GPU raises an interrupt when it finishes processing
 * that ID.  s_lastClId tracks the most recently completed ID so the CPU can
 * poll/wait for in-flight work.
 */

#include <eez/conf-internal.h>

#if defined(EEZ_NEMA_GFX)

#include <string.h>

#include <gpu2d.h>

#include <nema_hal.h>
#include <tsi_malloc.h>

#include <nema_graphics.h>

#include "nema_hal_stm32.h"

static bool nema_hal_stm32_init_tried = false;
bool nema_hal_stm32_ready = false;
nema_cmdlist_t nema_hal_stm32_cmd_list;

void nema_hal_stm32_init() {
    /*
     * One-time NemaGFX initialisation.
     *
     * nema_init() calls nema_sys_init() (our HAL implementation in
     * nema_hal_stm32.c) which sets up tsi_malloc and the ring buffer, then
     * performs GPU hardware initialisation.
     *
     * nema_cl_create() allocates a command list object.  A command list is a
     * GPU-side buffer of encoded draw commands.  We keep it as a static so we
     * can rewind and re-fill it every frame without re-allocating.
     *
     * If initialisation fails nema_hal_stm32_ready stays false and every subsequent call
     * returns early with no GPU activity.
     */
    if (!nema_hal_stm32_init_tried) {
        nema_hal_stm32_init_tried = true;
        if (nema_init() >= 0) {
            nema_hal_stm32_cmd_list = nema_cl_create();
            nema_hal_stm32_ready = true;
        }
    }
}

/*
 * NEMA_RING_SIZE_BYTES — size of the GPU command-list ring buffer.
 *
 * The ring buffer is a circular memory region in EXTRAM that the GPU reads
 * from.  1 KiB is sufficient for typical draw calls; increase if profiling
 * shows ring-full stalls.
 */
#define NEMA_RING_SIZE_BYTES 32 * 1024

/*
 * NEMA_POOL0_SIZE_BYTES — total size of tsi_malloc pool 0.
 *
 * 64 KiB carved out of EXTRAM, used for all GPU-visible allocations: the
 * ring buffer itself, command lists, and any temporary GPU buffers.
 * Increase if allocation failures (tsi_malloc returning NULL) are observed.
 */
#define NEMA_POOL0_SIZE_BYTES (32 * 64 * 1024)

/*
 * s_nemaPool0 — backing storage for tsi_malloc pool 0.
 *
 * EXTRAM_DATA places this array in external RAM so the GPU2D DMA engine can
 * reach it directly without going through a cache-coherency bridge.
 */
static EXTRAM_DATA uint8_t s_nemaPool0[NEMA_POOL0_SIZE_BYTES];

/*
 * s_nemaRing — the ring buffer descriptor used by NemaGFX.
 *
 * nema_rb_init() populates this struct after the backing buffer object
 * (s_nemaRing.bo) has been allocated from the pool.
 */
static nema_ringbuffer_t s_nemaRing;

/*
 * s_lastClId — ID of the most recently completed GPU command list.
 *
 * Declared volatile because it is written from the GPU2D completion ISR
 * (GPU2D_CommandListCpltCallback) and read from the main thread in
 * nema_wait_irq_cl().  Without volatile the compiler might cache the value
 * in a register and never see ISR updates.
 *
 * Initialised to -1 (no command list completed yet).
 */
static volatile int s_lastClId = -1;

/*
 * GPU2D_CommandListCpltCallback / HAL_GPU2D_CommandListCpltCallback
 * ─────────────────────────────────────────────────────────────────
 * These callbacks are invoked by the STM32 GPU2D HAL driver when the hardware
 * finishes processing a command list.  The GPU2D peripheral raises an
 * interrupt; the HAL driver's IRQ handler decodes the completed command-list
 * ID and calls back here.
 *
 * Two compilation paths exist:
 *  • USE_HAL_GPU2D_REGISTER_CALLBACKS == 1 (recommended):
 *    The application registers a function pointer with the HAL at runtime
 *    (HAL_GPU2D_RegisterCommandListCpltCallback).  The HAL calls the
 *    registered pointer instead of the weak default symbol.  This path
 *    registers GPU2D_CommandListCpltCallback and delegates from the weak
 *    HAL_GPU2D_CommandListCpltCallback override to it, keeping the ISR
 *    logic in one place.
 *
 *  • USE_HAL_GPU2D_REGISTER_CALLBACKS == 0 (default):
 *    The HAL provides a weak HAL_GPU2D_CommandListCpltCallback symbol.
 *    We override it directly here.  No function-pointer registration step.
 *
 * In both cases the logic is the same: record the completed command-list ID
 * in s_lastClId so nema_wait_irq_cl() can return as soon as the GPU is done.
 */
#if (USE_HAL_GPU2D_REGISTER_CALLBACKS == 1)
static void GPU2D_CommandListCpltCallback(GPU2D_HandleTypeDef *hgpu2d, uint32_t cmdListId) {
    (void)hgpu2d;
    s_lastClId = (int)cmdListId;
}
#endif

#if (USE_HAL_GPU2D_REGISTER_CALLBACKS == 1)
extern "C" void HAL_GPU2D_CommandListCpltCallback(GPU2D_HandleTypeDef *hgpu2d, uint32_t cmdListId) {
    GPU2D_CommandListCpltCallback(hgpu2d, cmdListId);
}
#else
extern "C" void HAL_GPU2D_CommandListCpltCallback(GPU2D_HandleTypeDef *hgpu2d, uint32_t cmdListId) {
    (void)hgpu2d;
    s_lastClId = (int)cmdListId;
}
#endif

/*
 * nema_sys_init — one-time initialisation of the NemaGFX subsystem.
 *
 * Called by nema_init() (part of the SDK) before any drawing can take place.
 * Must return 0 on success or a negative error code.
 *
 * Steps performed:
 *  1. Optionally register the command-list completion callback with the HAL
 *     so that ISR notifications are routed to our handler.
 *
 *  2. Initialise tsi_malloc pool 0 over s_nemaPool0:
 *       tsi_malloc_init_pool_aligned(poolId, virtPtr, physAddr, size,
 *                                    uniqueId, alignment)
 *     The virtual and physical addresses are identical here because the MCU
 *     uses a flat (no MMU) address space — the pointer value IS the physical
 *     address seen by the DMA engine.
 *     Alignment of 8 bytes satisfies the GPU2D's minimum buffer alignment.
 *
 *  3. Allocate the ring-buffer backing store from pool 0 via nema_buffer_create.
 *     nema_buffer_create returns a nema_buffer_t; we store it in s_nemaRing.bo.
 *     base_virt == NULL signals allocation failure.
 *
 *  4. Call nema_rb_init to hand the ring buffer to the SDK.  The second
 *     argument (1) means "reset the hardware ring-buffer registers".
 *
 *  5. Seed s_lastClId = 0 so that the first nema_wait_irq_cl(0) returns
 *     immediately rather than spinning forever.
 */
extern "C" int32_t nema_sys_init(void) {
    int32_t rc;

#if (USE_HAL_GPU2D_REGISTER_CALLBACKS == 1)
    /* Register our ISR-context callback with the HAL driver. */
    HAL_GPU2D_RegisterCommandListCpltCallback(&hgpu2d, GPU2D_CommandListCpltCallback);
#endif

    /* Initialise the pool allocator over the EXTRAM region. */
    rc = tsi_malloc_init_pool_aligned(0, (void *)s_nemaPool0, (uintptr_t)s_nemaPool0, NEMA_POOL0_SIZE_BYTES, 1, 8);
    if (rc != 0) {
        return rc;
    }

    /* Allocate the ring-buffer memory object from pool 0. */
    s_nemaRing.bo = nema_buffer_create(NEMA_RING_SIZE_BYTES);
    if (!s_nemaRing.bo.base_virt) {
        /* Allocation failed — not enough space in pool 0. */
        return -1;
    }

    /* Hand the ring buffer to the SDK and reset the HW ring registers. */
    rc = nema_rb_init(&s_nemaRing, 1);
    if (rc < 0) {
        return rc;
    }

    /*
     * Mark command list 0 as already done so the first wait call does not
     * spin.  The GPU hasn't executed anything yet, but there is nothing to
     * wait for either.
     */
    s_lastClId = 0;
    return 0;
}

/*
 * nema_wait_irq — wait for the GPU to raise a generic (non-CL) interrupt.
 *
 * On bare-metal STM32 without an RTOS scheduler driving GPU interrupts in a
 * DMA-completion fashion, there is nothing meaningful to wait on here; the
 * GPU2D peripheral on this SoC uses the command-list completion path
 * exclusively (see nema_wait_irq_cl below).  Return 0 immediately.
 */
extern "C" int nema_wait_irq(void) {
    return 0;
}

/*
 * nema_wait_irq_cl — block until the GPU has finished command list cl_id.
 *
 * After the CPU submits a command list (nema_cl_submit), it calls this
 * function to synchronise.  The GPU raises an interrupt on completion; the
 * ISR handler increments s_lastClId.  We busy-wait here until that value
 * reaches cl_id.
 *
 * Note: on an RTOS build this would ideally yield the task rather than
 * spin-wait, but the current single-threaded draw path keeps things simple.
 */
extern "C" int nema_wait_irq_cl(int cl_id) {
    while (s_lastClId < cl_id) {
    }
    return 0;
}

/*
 * nema_wait_irq_brk — wait for a GPU "break-point" interrupt.
 *
 * Break-points allow mid-command-list synchronisation points.  Not used in
 * this project; return immediately.
 */
extern "C" int nema_wait_irq_brk(int brk_id) {
    (void)brk_id;
    return 0;
}

/*
 * nema_reg_read / nema_reg_write — GPU register access.
 *
 * NemaGFX reads and writes GPU control/status registers through these two
 * functions.  On STM32 we delegate directly to the HAL GPU2D register
 * accessor functions which perform the memory-mapped I/O via the peripheral
 * base address stored in hgpu2d (the global GPU2D handle initialised by
 * MX_GPU2D_Init).
 *
 * 'reg' is a 32-bit byte offset into the GPU2D register bank as defined by
 * the NemaGFX register map header (nema_regs.h).
 */
extern "C" uint32_t nema_reg_read(uint32_t reg) {
    return HAL_GPU2D_ReadRegister(&hgpu2d, reg);
}

extern "C" void nema_reg_write(uint32_t reg, uint32_t value) {
    HAL_GPU2D_WriteRegister(&hgpu2d, reg, value);
}

/*
 * nema_host_malloc / nema_host_free — CPU-side (host) heap allocation.
 *
 * NemaGFX uses these for internal bookkeeping structures that do NOT need to
 * be GPU-visible (e.g. SDK-internal state structs).  We route them through
 * tsi_malloc so all NemaGFX memory comes from our managed pool rather than
 * the default system heap, making memory usage easy to audit.
 */
extern "C" void *nema_host_malloc(size_t size) {
    return tsi_malloc(size);
}

extern "C" void nema_host_free(void *ptr) {
    tsi_free(ptr);
}

/*
 * nema_buffer_t — GPU memory buffer descriptor (defined in nema_hal.h):
 *
 *   base_virt  CPU virtual address (pointer the CPU dereferences)
 *   base_phys  Physical/bus address the GPU DMA engine uses
 *   size       Allocation size in bytes
 *   fd         File descriptor (Linux) / tag field (bare-metal)
 *              We use fd == -1 as a "destroyed" sentinel.
 *
 * On this flat-address-space MCU, virtual == physical, so both fields hold
 * the same integer value.
 */

/*
 * nema_buffer_create — allocate a GPU-visible buffer from the default pool.
 *
 * Allocates 'size' bytes via tsi_malloc (pool 0) and fills in the buffer
 * descriptor.  The GPU can DMA to/from base_phys directly.
 *
 * Returns a zeroed descriptor on allocation failure (base_virt == NULL).
 */
extern "C" nema_buffer_t nema_buffer_create(int size) {
    nema_buffer_t bo;
    memset(&bo, 0, sizeof(bo));
    bo.base_virt = tsi_malloc(size);
    bo.base_phys = (uintptr_t)bo.base_virt; /* flat address space: virt == phys */
    bo.size = size;
    bo.fd = 0;
    return bo;
}

/*
 * nema_buffer_create_pool — allocate from a specific tsi_malloc pool.
 *
 * Identical to nema_buffer_create except the allocation comes from pool
 * 'pool' instead of the default pool.  Useful when multiple pools with
 * different backing memories (e.g. SRAM vs EXTRAM) are configured.
 */
extern "C" nema_buffer_t nema_buffer_create_pool(int pool, int size) {
    nema_buffer_t bo;
    memset(&bo, 0, sizeof(bo));
    bo.base_virt = tsi_malloc_pool(pool, size);
    bo.base_phys = (uintptr_t)bo.base_virt;
    bo.size = size;
    bo.fd = 0;
    return bo;
}

/*
 * nema_buffer_map — map a GPU buffer into CPU-addressable memory.
 *
 * On a system with separate GPU and CPU memory spaces this would perform an
 * mmap-like operation.  Here the buffer is already in CPU-accessible EXTRAM,
 * so we simply return the existing virtual pointer.
 */
extern "C" void *nema_buffer_map(nema_buffer_t *bo) {
    return bo->base_virt;
}

/*
 * nema_buffer_unmap — release a CPU mapping obtained via nema_buffer_map.
 *
 * Nothing to do on a flat address space.
 */
extern "C" void nema_buffer_unmap(nema_buffer_t *bo) {
    (void)bo;
}

/*
 * nema_buffer_destroy — free a GPU buffer and invalidate its descriptor.
 *
 * Guards against double-free by checking fd == -1 (the sentinel set at the
 * end of this function).  After destruction, base_virt and base_phys are
 * zeroed and size is 0, so any accidental use-after-free will fault cleanly
 * rather than silently corrupting data.
 */
extern "C" void nema_buffer_destroy(nema_buffer_t *bo) {
    if (bo->fd == -1) {
        /* Already destroyed — avoid double-free. */
        return;
    }

    if (bo->base_virt) {
        tsi_free(bo->base_virt);
    }

    /* Poison the descriptor so use-after-free is detectable. */
    bo->base_virt = 0;
    bo->base_phys = 0;
    bo->size = 0;
    bo->fd = -1;
}

/*
 * nema_buffer_phys — return the physical address of a GPU buffer.
 *
 * The NemaGFX SDK uses this when it needs to pass a buffer address to the
 * GPU hardware (e.g. as a texture base address or render target).  The GPU
 * DMA engine works with physical addresses, not CPU virtual pointers.
 */
extern "C" uintptr_t nema_buffer_phys(nema_buffer_t *bo) {
    return bo->base_phys;
}

/*
 * nema_buffer_flush — flush CPU caches for a buffer before GPU access.
 *
 * On a Cortex-M platform with a write-through or disabled cache (or when
 * the GPU-visible memory is mapped as Device/Strongly-Ordered), no explicit
 * flush is needed — writes are visible to DMA immediately.  If the cache
 * policy changes, a DSB + cache-clean operation should be added here.
 */
extern "C" void nema_buffer_flush(nema_buffer_t *bo) {
    (void)bo;
}

/*
 * nema_mutex_lock / nema_mutex_unlock — multi-threading guards.
 *
 * NemaGFX calls these around sections that must not be re-entered from
 * multiple threads.  This project currently uses the GPU from a single task,
 * so no locking is required.  If multi-threaded GPU access is ever added,
 * replace these stubs with an RTOS mutex (e.g. xSemaphoreTake / xSemaphoreGive).
 */
extern "C" int nema_mutex_lock(int mutex_id) {
    (void)mutex_id;
    return 0;
}

extern "C" int nema_mutex_unlock(int mutex_id) {
    (void)mutex_id;
    return 0;
}

/*
 * platform_disable_cache / platform_invalidate_cache
 * ───────────────────────────────────────────────────
 * Some NemaGFX SDK examples call these around DMA transfers to keep the CPU
 * data cache coherent with GPU-written memory.  On this platform the
 * GPU-visible EXTRAM is configured as non-cacheable (or cache is managed at
 * a higher level), so these are no-ops.  Add SCB_DisableDCache() /
 * SCB_InvalidateDCache() here if cache coherency issues arise.
 */
extern "C" void platform_disable_cache(void) {
}

extern "C" void platform_invalidate_cache(void) {
}

#endif
