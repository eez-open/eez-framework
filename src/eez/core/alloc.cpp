/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/heap.h>
#include <unistd.h>
#endif

#if defined(EEZ_FOR_LVGL)
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#endif

// EEZ-GUI on a PC (simulator, CM5, Studio): alloc is malloc
#if !defined(EEZ_FOR_LVGL) && !defined(EEZ_DASHBOARD_API) && (defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__) || defined(EEZ_PLATFORM_CM5_RASPI))
#define EEZ_ALLOC_FROM_C_HEAP 1
#else
#define EEZ_ALLOC_FROM_C_HEAP 0
#endif

#include <stdlib.h>
#include <stdint.h>

#include <eez/core/alloc.h>
#include <eez/core/os.h>

#if EEZ_ALLOC_FROM_C_HEAP && !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#endif

namespace eez {

#if defined(EEZ_FOR_LVGL)

uint32_t g_freeMemoryAtStart;
uint32_t g_allocMemoryAtStart;

void initAllocHeap(uint8_t *heap, size_t heapSize) {
    EEZ_UNUSED(heap);
    EEZ_UNUSED(heapSize);

	getAllocInfo(g_freeMemoryAtStart, g_allocMemoryAtStart);
}

void *alloc(size_t size, uint32_t id) {
    EEZ_UNUSED(id);
#if LVGL_VERSION_MAJOR >= 9
    return lv_malloc(size);
#else
    return lv_mem_alloc(size);
#endif
}

void free(void *ptr) {
#if LVGL_VERSION_MAJOR >= 9
    lv_free(ptr);
#else
    lv_mem_free(ptr);
#endif
}

template<typename T> void freeObject(T *ptr) {
	ptr->~T();
#if LVGL_VERSION_MAJOR >= 9
    lv_free(ptr);
#else
	lv_mem_free(ptr);
#endif
}

void getAllocInfo(uint32_t &free, uint32_t &alloc) {
#if defined(__EMSCRIPTEN__) && LV_USE_STDLIB_MALLOC == LV_STDLIB_CLIB
	size_t total_heap = emscripten_get_heap_size();
	size_t heap_break = (size_t)sbrk(0);

	free = total_heap - heap_break;
	alloc = heap_break - g_allocMemoryAtStart;
#else
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
	free = mon.free_size;
	alloc = mon.total_size - mon.free_size - g_allocMemoryAtStart;
#endif
}

#elif defined(EEZ_DASHBOARD_API)

void initAllocHeap(uint8_t *heap, size_t heapSize) {
}

void *alloc(size_t size, uint32_t id) {
    return ::malloc(size);
}

void free(void *ptr) {
    ::free(ptr);
}

template<typename T> void freeObject(T *ptr) {
	ptr->~T();
	::free(ptr);
}

void getAllocInfo(uint32_t &free, uint32_t &alloc) {
	free = emscripten_get_heap_max() - emscripten_get_heap_size();
	alloc = emscripten_get_heap_size();
}

#elif EEZ_ALLOC_FROM_C_HEAP

void initAllocHeap(uint8_t *heap, size_t heapSize) {
}

void *alloc(size_t size, uint32_t id) {
    return ::malloc(size);
}

void free(void *ptr) {
    ::free(ptr);
}

template<typename T> void freeObject(T *ptr) {
	ptr->~T();
	::free(ptr);
}

void getAllocInfo(uint32_t &free, uint32_t &alloc) {
	size_t allocBytes = 0;
	size_t freeBytes = 0;

#if defined(__EMSCRIPTEN__)
	allocBytes = emscripten_get_heap_size();
	freeBytes = emscripten_get_heap_max() - emscripten_get_heap_size();
#elif defined(_WIN32)
	// the private bytes of the process and the free physical memory
	PROCESS_MEMORY_COUNTERS_EX counters;
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&counters, sizeof(counters))) {
		allocBytes = counters.PrivateUsage;
	}
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	if (GlobalMemoryStatusEx(&status)) {
		freeBytes = (size_t)status.ullAvailPhys;
	}
#elif defined(__APPLE__)
	malloc_statistics_t statistics;
	malloc_zone_statistics(nullptr, &statistics);
	allocBytes = statistics.size_in_use;
	freeBytes = statistics.size_allocated - statistics.size_in_use;
#else
	struct mallinfo2 mi = mallinfo2();

	// bytes in use: heap chunks + mmap-ed chunks
	allocBytes = mi.uordblks + mi.hblkhd;

	// free: memory available to the system + free chunks retained by malloc
	freeBytes = mi.fordblks;
	FILE *fp = fopen("/proc/meminfo", "r");
	if (fp) {
		char line[128];
		unsigned long kb;
		while (fgets(line, sizeof(line), fp)) {
			if (sscanf(line, "MemAvailable: %lu kB", &kb) == 1) {
				freeBytes += (size_t)kb * 1024;
				break;
			}
		}
		fclose(fp);
	}
#endif

	free = freeBytes > UINT32_MAX ? UINT32_MAX : (uint32_t)freeBytes;
	alloc = allocBytes > UINT32_MAX ? UINT32_MAX : (uint32_t)allocBytes;
}

#else

static const size_t ALIGNMENT = 64;
static const size_t MIN_BLOCK_SIZE = 8;

struct AllocBlock {
	AllocBlock *next;
	int free;
	size_t size;
	uint32_t id;
};

static uint8_t *g_heap;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

#if EEZ_OPTION_THREADS
EEZ_MUTEX_DECLARE(alloc);
#endif

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

// initAssetsMemory sets up the heap before the first allocation. A later call,
// e.g. initAllocHeap(ALLOC_BUFFER, ALLOC_BUFFER_SIZE) by the application after
// initMemory, would drop everything allocated, so it is ignored.
void initAllocHeap(uint8_t *heap, size_t heapSize) {
    if (g_heap) {
        return;
    }

    g_heap = heap;

	AllocBlock *first = (AllocBlock *)g_heap;
	first->next = 0;
	first->free = 1;
	first->size = heapSize - sizeof(AllocBlock);

#if EEZ_OPTION_THREADS
	EEZ_MUTEX_CREATE(alloc);
#endif
}

void *alloc(size_t size, uint32_t id) {
	if (size == 0) {
		return nullptr;
	}

#if EEZ_OPTION_THREADS
	if (EEZ_MUTEX_WAIT(alloc, WAIT_FOREVER)) {
#endif

		AllocBlock *firstBlock = (AllocBlock *)g_heap;

		AllocBlock *block = firstBlock;
		size = ((size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;

		// find free block with enough size
		// TODO merge multiple free consecutive blocks into one that has enough size
		while (block) {
			if (block->free && block->size >= size) {
				break;
			}
			block = block->next;
		}

		if (!block) {
#if EEZ_OPTION_THREADS
			EEZ_MUTEX_RELEASE(alloc);
#endif
			return nullptr;
		}

		int remainingSize = block->size - size - sizeof(AllocBlock);
		if (remainingSize >= (int)MIN_BLOCK_SIZE) {
			// remainingSize is enough to create a new block
			auto newBlock = (AllocBlock *)((uint8_t *)block + sizeof(AllocBlock) + size);
			newBlock->next = block->next;
			newBlock->free = 1;
			newBlock->size = remainingSize;

			block->next = newBlock;
			block->size = size;
		}

		block->free = 0;
		block->id = id;

#if EEZ_OPTION_THREADS
		EEZ_MUTEX_RELEASE(alloc);
#endif

		return block + 1;

#if EEZ_OPTION_THREADS
	}
#endif

	return nullptr;
}

void free(void *ptr) {
	if (ptr == 0) {
		return;
	}

#if EEZ_OPTION_THREADS
	if (EEZ_MUTEX_WAIT(alloc, WAIT_FOREVER)) {
#endif

		AllocBlock *firstBlock = (AllocBlock *)g_heap;

		AllocBlock *prevBlock = nullptr;
		AllocBlock *block = firstBlock;

		while (block && block + 1 < ptr) {
			prevBlock = block;
			block = block->next;
		}

		if (!block || block + 1 != ptr || block->free) {
			assert(false);
#if EEZ_OPTION_THREADS
			EEZ_MUTEX_RELEASE(alloc);
#endif
			return;
		}

		// reset memory to catch errors when memory is used after free is called
		memset(ptr, 0xCC, block->size);

		auto nextBlock = block->next;
		if (nextBlock && nextBlock->free) {
			if (prevBlock && prevBlock->free) {
				// both next and prev blocks are free, merge 3 blocks into one
				prevBlock->next = nextBlock->next;
				prevBlock->size += sizeof(AllocBlock) + block->size + sizeof(AllocBlock) + nextBlock->size;
			} else {
				// next block is free, merge 2 blocks into one
				block->next = nextBlock->next;
				block->size += sizeof(AllocBlock) + nextBlock->size;
				block->free = 1;
			}
		} else if (prevBlock && prevBlock->free) {
			// prev block is free, merge 2 blocks into one
			prevBlock->next = nextBlock;
			prevBlock->size += sizeof(AllocBlock) + block->size;
		} else {
			// just free
			block->free = 1;
		}

#if EEZ_OPTION_THREADS
		EEZ_MUTEX_RELEASE(alloc);
	}
#endif
}

template<typename T> void freeObject(T *ptr) {
	ptr->~T();
	free(ptr);
}

#if OPTION_SCPI
void dumpAlloc(scpi_t *context) {
	AllocBlock *first = (AllocBlock *)g_heap;
	AllocBlock *block = first;
	while (block) {
		char buffer[100];
		if (block->free) {
			snprintf(buffer, sizeof(buffer), "FREE: %d", (int)block->size);
		} else {
			snprintf(buffer, sizeof(buffer), "ALOC (0x%08x): %d", (unsigned int)block->id, (int)block->size);
		}
		SCPI_ResultText(context, buffer);
		block = block->next;
	}
}
#endif

void getAllocInfo(uint32_t &free, uint32_t &alloc) {
	free = 0;
	alloc = 0;

#if EEZ_OPTION_THREADS
	if (EEZ_MUTEX_WAIT(alloc, WAIT_FOREVER)) {
#endif

		AllocBlock *first = (AllocBlock *)g_heap;
		AllocBlock *block = first;
		while (block) {
			if (block->free) {
				free += block->size;
			} else {
				alloc += block->size;
			}
			block = block->next;
		}

#if EEZ_OPTION_THREADS
		EEZ_MUTEX_RELEASE(alloc);
	}
#endif
}

#endif

} // eez
