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

#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__) || defined(EEZ_PLATFORM_CM5_RASPI)

#ifndef __EMSCRIPTEN__
#include <chrono>
#endif

#if EEZ_OPTION_THREADS

#include "cmsis_os2.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef __EMSCRIPTEN__
#include <thread>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#endif
#endif

////////////////////////////////////////////////////////////////////////////////

#ifdef __EMSCRIPTEN__
#define MAX_THREADS 100
struct Thread {
	osThreadFunc_t func;
};

static std::vector<Thread *> g_threads;
static Thread *g_currentThread;
#endif

osThreadId_t osThreadNew(osThreadFunc_t func, void *, const osThreadAttr_t *attr) {
#if defined(__EMSCRIPTEN__)
    Thread *thread = new Thread;
    thread->func = func;
    g_threads.push_back(thread);
    return thread;
#else
    auto t = new std::thread(func, nullptr);
#if defined(__linux__)
    // shown by profilers, debuggers and top -H, max 15 characters
    if (attr && attr->name) {
        char name[16];
        snprintf(name, sizeof(name), "%s", attr->name);
        pthread_setname_np(t->native_handle(), name);
    }
#endif
    return t->get_id();
#endif
}

osStatus osThreadTerminate(osThreadId_t thread_id) {
#if defined(__EMSCRIPTEN__)
    auto it = find(g_threads.begin(), g_threads.end(), (Thread *)thread_id);
    if (it == g_threads.end()) {
        return osError;
    }
    g_threads.erase(it);
	return osOK;
#else
    // TODO
	return osOK;
#endif
}

osThreadId_t osThreadGetId() {
#if defined(__EMSCRIPTEN__)
    return g_currentThread;
#else
    return std::this_thread::get_id();
#endif
}

#ifdef __EMSCRIPTEN__
static void oneIter() {
    for(Thread *thread: g_threads) {
        g_currentThread = thread;
        thread->func(nullptr);
    }
}

void eez_system_tick() {
    for (int i = 0; i < 2; i++) {
        oneIter();
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

osStatus osKernelInitialize(void) {
	return osOK;
}

osStatus osKernelStart(void) {
    return osOK;
}

osStatus osDelay(uint32_t millisec) {
#ifndef __EMSCRIPTEN__
    std::this_thread::sleep_for(std::chrono::milliseconds(millisec));
#endif
    return osOK;
}

////////////////////////////////////////////////////////////////////////////////

osMutexId_t osMutexNew(const osMutexAttr_t *) {
#ifdef __EMSCRIPTEN__
    return true;
#else
    return new std::mutex;
#endif
}

osStatus osMutexAcquire(osMutexId_t mutex, unsigned int timeout) {
#ifndef __EMSCRIPTEN__
	mutex->lock();
#endif
    return osOK;
}

osStatus osMutexRelease(osMutexId_t mutex) {
#ifndef __EMSCRIPTEN__
	mutex->unlock();
#endif
	return osOK;
}

////////////////////////////////////////////////////////////////////////////////

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const void *) {
	auto queue = new MessageQueue();
	queue->elementSize = msg_size;
    return queue;
}

osStatus osMessageQueueGet(osMessageQueueId_t queue, void *msg_ptr, uint8_t *, uint32_t timeout) {
#ifdef __EMSCRIPTEN__
    if (queue->elements.empty()) {
        return osError;
    }
#else
    std::unique_lock<std::mutex> lock(queue->mutex);

    // Wait for the element without polling (osDelay loop), so the message
    // is received as soon as it is put. Timeout 0 doesn't wait.
    auto hasElement = [queue] { return !queue->elements.empty(); };
    if (timeout == 0) {
        if (!hasElement()) {
            return osError;
        }
    } else if (timeout == osWaitForever) {
        queue->elementPut.wait(lock, hasElement);
    } else if (!queue->elementPut.wait_for(lock, std::chrono::milliseconds(timeout), hasElement)) {
        return osError;
    }
#endif

    auto data = queue->elements.front();
    queue->elements.pop();
    memcpy(msg_ptr, data, queue->elementSize);
    delete [] data;

    return osOK;
}

osStatus osMessageQueuePut(osMessageQueueId_t queue, const void *msg_ptr, uint8_t, uint32_t timeout) {
#ifndef __EMSCRIPTEN__
    queue->mutex.lock();
#endif

    uint8_t *data = new uint8_t[queue->elementSize];
	memcpy(data, msg_ptr, queue->elementSize);
	queue->elements.push(data);

#ifndef __EMSCRIPTEN__
    queue->mutex.unlock();
    queue->elementPut.notify_one();
#endif

    return osOK;
}

#endif // EEZ_OPTION_THREADS

uint32_t osKernelGetTickCount() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}


#endif // defined(EEZ_PLATFORM_SIMULATOR)
