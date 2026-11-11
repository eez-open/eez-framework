/*
 * eez-framework tests: core/os (eez::Thread, MessageQueue, the thread macros,
 * millis) and core/hmi (inactivity period)
 */

#include <pthread.h>
#include <string.h>

#include <chrono>
#include <string>
#include <thread>

#include <eez/conf-internal.h>
#include <eez/core/os.h>
#include <eez/core/hmi.h>

#include "harness.h"
#include "test.h"

using namespace eez;

// the framework's millis(), behind the fake clock (--wrap of the test link)
extern "C" uint32_t __real__ZN3eez6millisEv();

namespace {

// steady clock when the test program was loaded
const std::chrono::steady_clock::time_point g_programStart = std::chrono::steady_clock::now();

} // namespace

////////////////////////////////////////////////////////////////////////////////
// MessageQueue

TEST("core/os: MessageQueue is first in, first out") {
    MessageQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);
    int message = 0;
    CHECK(queue.get(message, 0));
    CHECK_EQ(message, 1);
    CHECK(queue.get(message, 0));
    CHECK_EQ(message, 2);
    CHECK(queue.get(message, WAIT_FOREVER));
    CHECK_EQ(message, 3);
}

TEST("core/os: MessageQueue get of an empty queue returns false") {
    MessageQueue<int> queue;
    int message = 77;
    CHECK(!queue.get(message, 0));
    CHECK(!queue.get(message, 1));
    CHECK_EQ(message, 77);
}

TEST("core/os: MessageQueue copies the message") {
    struct Message {
        int type;
        char text[16];
    };
    MessageQueue<Message> queue;
    Message m = { 5, "hello" };
    queue.put(m);
    strcpy(m.text, "changed");
    Message out = {};
    CHECK(queue.get(out, 0));
    CHECK_EQ(out.type, 5);
    CHECK_STR(out.text, "hello");
}

////////////////////////////////////////////////////////////////////////////////
// Thread

namespace {

Thread g_thread;
MessageQueue<int> g_threadQueue;

} // namespace

TEST("core/os: Thread runs its function") {
    g_thread.start("core_test", [] {
        g_threadQueue.put(42);
    });
    int message = 0;
    CHECK(g_threadQueue.get(message, WAIT_FOREVER));
    CHECK_EQ(message, 42);
}

TEST("core/os: Thread isCurrent is true only on the thread") {
    g_thread.start("core_test", [] {
        g_threadQueue.put(g_thread.isCurrent() ? 1 : 0);
    });
    int message = -1;
    CHECK(g_threadQueue.get(message, WAIT_FOREVER));
    CHECK_EQ(message, 1);
    CHECK(!g_thread.isCurrent());
}

TEST("core/os: Thread attach makes the calling thread current") {
    static Thread thread;
    CHECK(!thread.isCurrent());
    thread.attach(std::this_thread::get_id());
    CHECK(thread.isCurrent());
}

namespace {

MessageQueue<std::string> g_nameQueue;

} // namespace

TEST("core/os: Thread name is cut to 15 characters") {
    static Thread thread;
    thread.start("core_test_thread_name", [] {
        char name[32] = "";
        pthread_getname_np(pthread_self(), name, sizeof(name));
        g_nameQueue.put(name);
    });
    std::string name;
    CHECK(g_nameQueue.get(name, WAIT_FOREVER));
    CHECK_EQ(name, "core_test_threa");
}

namespace {

MessageQueue<int> g_producerQueue;
Thread g_producer1;
Thread g_producer2;

} // namespace

TEST("core/os: MessageQueue from two threads loses no message") {
    g_producer1.start("producer1", [] {
        for (int i = 0; i < 1000; i++) {
            g_producerQueue.put(1);
        }
    });
    g_producer2.start("producer2", [] {
        for (int i = 0; i < 1000; i++) {
            g_producerQueue.put(2);
        }
    });
    int count1 = 0, count2 = 0;
    for (int i = 0; i < 2000; i++) {
        int message;
        REQUIRE(g_producerQueue.get(message, WAIT_FOREVER));
        if (message == 1) count1++;
        if (message == 2) count2++;
    }
    CHECK_EQ(count1, 1000);
    CHECK_EQ(count2, 1000);
    int message;
    CHECK(!g_producerQueue.get(message, 0));
}

////////////////////////////////////////////////////////////////////////////////
// the thread macros

namespace {

EEZ_THREAD_DECLARE(coreTestMacro, Normal, 1024);
EEZ_MESSAGE_QUEUE_DECLARE(coreTestMacro, { int value; });
EEZ_MUTEX_DECLARE(coreTestMacro);

void coreTestMacroThread(void *) {
    coreTestMacroMessageQueueObject obj;
    obj.value = EEZ_THREAD_IS_CURRENT(coreTestMacro) ? 7 : -7;
    EEZ_MESSAGE_QUEUE_PUT(coreTestMacro, obj, WAIT_FOREVER);
}

} // namespace

TEST("core/os: thread, message queue and mutex macros") {
    EEZ_MESSAGE_QUEUE_CREATE(coreTestMacro, 10);
    EEZ_MUTEX_CREATE(coreTestMacro);
    EEZ_THREAD_CREATE(coreTestMacro, coreTestMacroThread);

    coreTestMacroMessageQueueObject obj;
    CHECK(EEZ_MESSAGE_QUEUE_GET(coreTestMacro, obj, WAIT_FOREVER));
    CHECK_EQ(obj.value, 7);
    CHECK(!EEZ_THREAD_IS_CURRENT(coreTestMacro));

    CHECK(EEZ_MUTEX_WAIT(coreTestMacro, WAIT_FOREVER));
    EEZ_MUTEX_RELEASE(coreTestMacro);
}

////////////////////////////////////////////////////////////////////////////////
// millis

TEST("core/os: millis of the framework is the fake clock in the tests") {
    uint32_t start = millis();
    CHECK_EQ(start, harness::now());
    harness::advance(123);
    CHECK_EQ(millis() - start, 123u);
}

TEST("core/os: the real millis counts milliseconds") {
    uint32_t a = __real__ZN3eez6millisEv();
    // busy wait for 20 ms of the steady clock
    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
    while (std::chrono::steady_clock::now() < until) {
    }
    uint32_t b = __real__ZN3eez6millisEv();
    CHECK(b - a >= 20);
    CHECK(b - a < 10000);
}

TEST_BUG("core/os: the real millis counts from the start of the program", "C13") {
    // it is the OS uptime (steady clock epoch) truncated to 32 bits: it wraps
    // 49.7 days after boot, not after the program start
    using namespace std::chrono;
    uint32_t real = __real__ZN3eez6millisEv();
    uint32_t sinceStart = (uint32_t)duration_cast<milliseconds>(steady_clock::now() - g_programStart).count();
    CHECK(real <= sinceStart + 1000);
}

////////////////////////////////////////////////////////////////////////////////
// hmi

TEST("core/hmi: inactivity period counts from the last activity") {
    hmi::noteActivity();
    CHECK_EQ(hmi::getInactivityPeriodMs(), 0u);
    CHECK_EQ(hmi::getTimeOfLastActivity(), harness::now());
    harness::advance(1500);
    CHECK_EQ(hmi::getInactivityPeriodMs(), 1500u);
    hmi::tick();
    CHECK_EQ(hmi::getInactivityPeriodMs(), 1500u);
}

TEST("core/hmi: inactivity period is capped at 60 s by tick") {
    hmi::noteActivity();
    harness::advance(61000);
    // pinned: until tick() runs, the period grows past the cap
    CHECK_EQ(hmi::getInactivityPeriodMs(), 61000u);
    hmi::tick();
    CHECK_EQ(hmi::getInactivityPeriodMs(), 60000u);
    harness::advance(100000);
    CHECK_EQ(hmi::getInactivityPeriodMs(), 60000u);
    // pinned: once capped, the time of the last activity moves with the clock
    CHECK_EQ(hmi::getTimeOfLastActivity(), harness::now() - 60000);
}

TEST("core/hmi: activity after the cap starts again from 0") {
    hmi::noteActivity();
    harness::advance(70000);
    hmi::tick();
    hmi::noteActivity();
    CHECK_EQ(hmi::getInactivityPeriodMs(), 0u);
    harness::advance(10);
    CHECK_EQ(hmi::getInactivityPeriodMs(), 10u);
}

TEST("core/hmi: a touch is activity") {
    harness::advance(70000);
    hmi::tick();
    CHECK_EQ(hmi::getInactivityPeriodMs(), 60000u);
    harness::touch(400, 300);
    // the touch reaches the event processing in the first frame
    harness::frame(4);
    CHECK(hmi::getInactivityPeriodMs() <= 2 * harness::FRAME_MS);
    harness::release();
    harness::frame(2);
}
