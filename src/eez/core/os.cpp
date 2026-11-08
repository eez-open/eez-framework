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

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#endif

#if defined(EEZ_PLATFORM_ESP32)
#include <esp_timer.h>
#endif

#if defined(EEZ_PLATFORM_PICO)
#include "pico/stdlib.h"
#endif

#if defined(EEZ_PLATFORM_RASPBERRY)
#include <circle/timer.h>
#endif

#if defined(EEZ_FOR_LVGL)
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#endif

#if defined(__EMSCRIPTEN__)
#include <sys/time.h>
#include <vector>
#endif

#if defined(__linux__)
#include <pthread.h>
#include <stdio.h>
#endif

#include <eez/core/os.h>

namespace eez {

uint32_t millis() {
#if defined(EEZ_PLATFORM_STM32)
	return HAL_GetTick();
#elif defined(__EMSCRIPTEN__)
	return (uint32_t)emscripten_get_now();
#elif defined(EEZ_PLATFORM_SIMULATOR) || defined(EEZ_PLATFORM_CM5_RASPI)
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#elif defined(EEZ_PLATFORM_ESP32)
	return (unsigned long) (esp_timer_get_time() / 1000ULL);
#elif defined(EEZ_PLATFORM_PICO)
    auto abs_time = get_absolute_time();
    return to_ms_since_boot(abs_time);
#elif defined(EEZ_PLATFORM_RASPBERRY)
    unsigned nStartTicks = CTimer::Get()->GetClockTicks();
    return nStartTicks / 1000;
#elif defined(EEZ_FOR_LVGL)
    return lv_tick_get();
#else
    #error "Missing millis implementation";
#endif
}

void sleepMs(uint32_t ms) {
#if defined(__EMSCRIPTEN__)
    (void)ms;
#elif EEZ_OPTION_THREADS && defined(EEZ_PLATFORM_STM32)
    osDelay(ms);
#elif EEZ_OPTION_THREADS
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#elif defined(EEZ_PLATFORM_STM32)
    HAL_Delay(ms);
#else
    (void)ms;
#endif
}

#if EEZ_OPTION_THREADS && !defined(EEZ_PLATFORM_STM32) && !defined(EEZ_FOR_LVGL)

#if defined(__EMSCRIPTEN__)

struct EmscriptenThread {
    Thread *thread;
    std::function<void()> func;
};

static std::vector<EmscriptenThread> g_threads;
static const Thread *g_currentThread;

void Thread::start(const char *, std::function<void()> func) {
    g_threads.push_back({ this, func });
}

bool Thread::isCurrent() const {
    return g_currentThread == this;
}

#else

void Thread::start(const char *name, std::function<void()> func) {
    std::thread thread([this, name, func] {
        m_id = std::this_thread::get_id();
#if defined(__linux__)
        // at most 15 characters
        char threadName[16];
        snprintf(threadName, sizeof(threadName), "%s", name);
        pthread_setname_np(pthread_self(), threadName);
#else
        (void)name;
#endif
        func();
    });
    // also here, so isCurrent() is right before the thread runs
    m_id = thread.get_id();
    thread.detach();
}

bool Thread::isCurrent() const {
    return std::this_thread::get_id() == m_id.load();
}

#endif

#endif

} // namespace eez

#if defined(__EMSCRIPTEN__) && EEZ_OPTION_THREADS && !defined(EEZ_FOR_LVGL)
// called from the Emscripten main loop: one iteration of every thread, twice
void eez_system_tick() {
    for (int i = 0; i < 2; i++) {
        for (auto &thread : eez::g_threads) {
            eez::g_currentThread = thread.thread;
            thread.func();
        }
    }
}
#endif
