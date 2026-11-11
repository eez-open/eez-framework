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

#include <stdlib.h>
#include <string.h>

#include <eez/core/memory.h>
#include <eez/core/alloc.h>

#if defined(EEZ_FOR_LVGL)
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#endif

#if defined(EEZ_DASHBOARD_API)
#include <emscripten/heap.h>
#endif

namespace eez {

uint8_t *ALLOC_BUFFER = 0;
uint32_t ALLOC_BUFFER_SIZE = 0;

void initMemory() {
    initAssetsMemory();
    initOtherMemory();
}

void initAssetsMemory() {
#if defined(EEZ_FOR_LVGL)
#if defined(LV_MEM_SIZE)
    ALLOC_BUFFER_SIZE = LV_MEM_SIZE;
#endif
#elif defined(EEZ_DASHBOARD_API)
    ALLOC_BUFFER_SIZE = emscripten_get_heap_max();
#elif defined(EEZ_PLATFORM_STM32)
    ALLOC_BUFFER = MEMORY_BEGIN;
    ALLOC_BUFFER_SIZE = MEMORY_SIZE;
    initAllocHeap(ALLOC_BUFFER, ALLOC_BUFFER_SIZE);
#endif
}

void initOtherMemory() {
}

uint8_t *allocBuffer(uint32_t size) {
#if defined(EEZ_FOR_LVGL)
#if LVGL_VERSION_MAJOR >= 9
    auto buffer = (uint8_t *)lv_malloc(size);
#else
    auto buffer = (uint8_t *)lv_mem_alloc(size);
#endif
#elif defined(EEZ_DASHBOARD_API)
    auto buffer = (uint8_t *)::malloc(size);
#else
    static const uintptr_t ALIGNMENT = 64;
    auto memory = (uint8_t *)alloc(size + ALIGNMENT - 1, 0x2d6e8f1a);
    auto buffer = memory ? (uint8_t *)(((uintptr_t)memory + ALIGNMENT - 1) & ~(ALIGNMENT - 1)) : nullptr;
#endif
    if (buffer) {
        memset(buffer, 0, size);
    }
    return buffer;
}

} // eez
