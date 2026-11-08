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

#include <atomic>
#include <stdint.h>

namespace eez {

// Timeout of the message queue and mutex functions: wait without a limit.
static const uint32_t WAIT_FOREVER = 0xFFFFFFFFU;

} // namespace eez

#if defined(EEZ_FOR_LVGL)

uint32_t osKernelGetTickCount(void);

#elif EEZ_OPTION_THREADS && defined(EEZ_PLATFORM_STM32)

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

#define EEZ_THREAD_DECLARE(NAME, PRIORITY, STACK_SIZE) \
    osThreadId_t g_##NAME##TaskHandle; \
    const osThreadAttr_t g_##NAME##TaskAttributes = { \
        #NAME, \
        0, \
        0, \
        0, \
        0, \
        STACK_SIZE, \
        osPriority##PRIORITY, \
        0, \
        0, \
    }
#define EEZ_THREAD_CREATE(NAME, THREAD_FUNC) g_##NAME##TaskHandle = osThreadNew(THREAD_FUNC, nullptr, &g_##NAME##TaskAttributes);
#define EEZ_THREAD_TERMINATE(NAME) osThreadTerminate(g_##NAME##TaskHandle)
#define EEZ_THREAD_IS_CURRENT(NAME) (osThreadGetId() == g_##NAME##TaskHandle)

#define EEZ_MESSAGE_QUEUE_DECLARE(NAME, OBJECT_DEF) \
    struct NAME##MessageQueueObject OBJECT_DEF; \
    osMessageQueueId_t g_##NAME##MessageQueueId
#define EEZ_MESSAGE_QUEUE_CREATE(NAME, QUEUE_SIZE) g_##NAME##MessageQueueId = osMessageQueueNew(QUEUE_SIZE, sizeof(NAME##MessageQueueObject), nullptr)
#define EEZ_MESSAGE_QUEUE_GET(NAME, OBJ, TIMEOUT) (osMessageQueueGet(g_##NAME##MessageQueueId, &OBJ, nullptr, TIMEOUT) == osOK)
#define EEZ_MESSAGE_QUEUE_PUT(NAME, OBJ, TIMEOUT) osMessageQueuePut(g_##NAME##MessageQueueId, &OBJ, 0, TIMEOUT)

#define EEZ_MUTEX_DECLARE(NAME) \
    osMutexId_t g_##NAME##mutexId;\
    const osMutexAttr_t g_##NAME##mutexAttr = { \
        #NAME, \
        osMutexRecursive | osMutexPrioInherit, \
        NULL, \
        0U \
    }
#define EEZ_MUTEX_CREATE(NAME) g_##NAME##mutexId = osMutexNew(&g_##NAME##mutexAttr)
#define EEZ_MUTEX_WAIT(NAME, TIMEOUT) osMutexAcquire(g_##NAME##mutexId, TIMEOUT) == osOK
#define EEZ_MUTEX_RELEASE(NAME) osMutexRelease(g_##NAME##mutexId)

#elif EEZ_OPTION_THREADS

// C++ threads (simulator, Linux and Emscripten). The priority and the stack
// size of a thread and the size of a message queue are not used.

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace eez {

// A detached thread with a name (shown by debuggers and top -H). Emscripten
// has no threads: there the thread function is called once in every main
// loop iteration (eez_system_tick), so it must do one iteration and return.
class Thread {
public:
    void start(const char *name, std::function<void()> func);

    // the calling thread is this thread
    bool isCurrent() const;

#ifndef __EMSCRIPTEN__
    // the thread with this id is this thread (e.g. the GUI loop that runs on
    // the process main thread)
    void attach(std::thread::id id) { m_id = id; }

private:
    std::atomic<std::thread::id> m_id;
#endif
};

// Messages between threads. The queue has no size limit, so put never waits.
template <typename T>
class MessageQueue {
public:
    void put(const T &message) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_messages.push_back(message);
        }
        m_messagePut.notify_one();
    }

    // Takes the oldest message. Waits for it at most timeoutMs: WAIT_FOREVER
    // waits without a limit and 0 doesn't wait (Emscripten never waits).
    // Returns false if there is no message.
    bool get(T &message, uint32_t timeoutMs) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto hasMessage = [this] { return !m_messages.empty(); };
#ifndef __EMSCRIPTEN__
        if (timeoutMs == WAIT_FOREVER) {
            m_messagePut.wait(lock, hasMessage);
        } else if (timeoutMs > 0) {
            m_messagePut.wait_for(lock, std::chrono::milliseconds(timeoutMs), hasMessage);
        }
#endif
        if (!hasMessage()) {
            return false;
        }
        message = m_messages.front();
        m_messages.pop_front();
        return true;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_messagePut;
    std::deque<T> m_messages;
};

} // namespace eez

#define EEZ_THREAD_DECLARE(NAME, PRIORITY, STACK_SIZE) eez::Thread g_##NAME##Thread
#define EEZ_THREAD_CREATE(NAME, THREAD_FUNC) g_##NAME##Thread.start(#NAME, [] { THREAD_FUNC(nullptr); })
#define EEZ_THREAD_IS_CURRENT(NAME) g_##NAME##Thread.isCurrent()

#define EEZ_MESSAGE_QUEUE_DECLARE(NAME, OBJECT_DEF) \
    struct NAME##MessageQueueObject OBJECT_DEF; \
    eez::MessageQueue<NAME##MessageQueueObject> g_##NAME##MessageQueue
#define EEZ_MESSAGE_QUEUE_CREATE(NAME, QUEUE_SIZE)
#define EEZ_MESSAGE_QUEUE_GET(NAME, OBJ, TIMEOUT) g_##NAME##MessageQueue.get(OBJ, TIMEOUT)
#define EEZ_MESSAGE_QUEUE_PUT(NAME, OBJ, TIMEOUT) g_##NAME##MessageQueue.put(OBJ)

#define EEZ_MUTEX_DECLARE(NAME) std::mutex g_##NAME##Mutex
#define EEZ_MUTEX_CREATE(NAME)
#define EEZ_MUTEX_WAIT(NAME, TIMEOUT) (g_##NAME##Mutex.lock(), true)
#define EEZ_MUTEX_RELEASE(NAME) g_##NAME##Mutex.unlock()

#endif

#if defined(__EMSCRIPTEN__)
    #ifndef EM_PORT_API
        #include <emscripten.h>
        #if defined(__cplusplus)
            #define EM_PORT_API(rettype) extern "C" rettype EMSCRIPTEN_KEEPALIVE
        #else
            #define EM_PORT_API(rettype) rettype EMSCRIPTEN_KEEPALIVE
        #endif
    #endif
#else
    #define EM_PORT_API(rettype) rettype
#endif

namespace eez {

enum TestResult {
	TEST_NONE,
	TEST_FAILED,
	TEST_OK,
	TEST_CONNECTING,
	TEST_SKIPPED,
	TEST_WARNING
};

uint32_t millis();

// Sleeps the calling thread (osDelay on STM32, nothing on Emscripten).
void sleepMs(uint32_t ms);

#if EEZ_OPTION_THREADS
extern std::atomic_bool g_shutdown;
#endif

void shutdown();

} // namespace eez
