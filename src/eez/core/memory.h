/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <eez/conf-internal.h>

#include <stdint.h>

// Everything is allocated from the heap of alloc() (eez/core/alloc.h):
// - LVGL: lv_malloc,
// - Dashboard and EEZ-GUI on a PC (simulator, CM5, Studio): the C heap,
// - EEZ-GUI on STM32: the memory from MEMORY_BEGIN of MEMORY_SIZE bytes
//   (usually SDRAM), set up by initAssetsMemory before the first allocation.

namespace eez {

#ifdef CONF_MEMORY_BEGIN
    static uint8_t * const MEMORY_BEGIN = (uint8_t *)CONF_MEMORY_BEGIN;
#else
    #if defined(EEZ_FOR_LVGL) || defined(EEZ_DASHBOARD_API)
        static uint8_t * const MEMORY_BEGIN = 0;
    #elif defined(EEZ_PLATFORM_STM32)
        static uint8_t * const MEMORY_BEGIN = (uint8_t *)0xc0000000u;
    #endif
#endif

#ifdef CONF_MEMORY_SIZE
    static const uint32_t MEMORY_SIZE = CONF_MEMORY_SIZE;
#else
    #if defined(EEZ_PLATFORM_STM32) && !defined(EEZ_FOR_LVGL) && !defined(EEZ_DASHBOARD_API)
        #if CONF_OPTION_FPGA
            static const uint32_t MEMORY_SIZE = 32 * 1024 * 1024;
        #elif defined(EEZ_PLATFORM_STM32F469I_DISCO)
            static const uint32_t MEMORY_SIZE = 16 * 1024 * 1024;
        #else
            static const uint32_t MEMORY_SIZE = 8 * 1024 * 1024;
        #endif
    #endif
#endif

// LVGL: LV_MEM_SIZE, Dashboard: the maximum heap size, EEZ-GUI on STM32: the
// heap (MEMORY_BEGIN, MEMORY_SIZE), EEZ-GUI on a PC: 0
extern uint8_t *ALLOC_BUFFER;
extern uint32_t ALLOC_BUFFER_SIZE;

void initMemory();
// sets up the heap (EEZ-GUI on STM32)
void initAssetsMemory();
// nothing to do: the buffers are allocated where they are used
void initOtherMemory();

// A buffer that is never freed: zero-filled and, in EEZ-GUI, aligned to 64
// bytes (DMA, the cache lines of the STM32F7/H7). nullptr if there is not
// enough memory.
uint8_t *allocBuffer(uint32_t size);

} // eez
