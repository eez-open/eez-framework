/*
 * eez-framework tests: core/memory (allocBuffer, no memory pool on a PC) and
 * core/alloc on the CM5 path (alloc and free are malloc and free)
 */

#include <stdint.h>
#include <string.h>

#include <eez/conf-internal.h>
#include <eez/core/alloc.h>
#include <eez/core/memory.h>
#include <eez/core/value.h>

#include "test.h"

using namespace eez;

namespace {

const uint32_t KB = 1024;

} // namespace

////////////////////////////////////////////////////////////////////////////////
// memory

TEST("core/memory: EEZ-GUI on a PC has no memory pool") {
    CHECK(ALLOC_BUFFER == nullptr);
    CHECK_EQ(ALLOC_BUFFER_SIZE, 0u);
}

TEST("core/memory: allocBuffer gives a zero-filled buffer aligned to 64 bytes") {
    const uint32_t sizes[] = { 1, 63, 64, 1000, 100 * KB };
    for (uint32_t size : sizes) {
        uint8_t *buffer = allocBuffer(size);
        REQUIRE(buffer != nullptr);
        CHECK_EQ((uint64_t)((uintptr_t)buffer % 64), (uint64_t)0);
        uint32_t nonZero = 0;
        for (uint32_t i = 0; i < size; i++) {
            nonZero += buffer[i] != 0;
        }
        CHECK_EQ(nonZero, 0u);
        // the whole buffer can be written (AddressSanitizer)
        memset(buffer, 0xA5, size);
    }
}

TEST("core/memory: the buffers of allocBuffer don't overlap") {
    uint8_t *a = allocBuffer(KB);
    uint8_t *b = allocBuffer(KB);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a + KB <= b || b + KB <= a);
}

TEST("core/memory: initAllocHeap does nothing on a PC") {
    uint8_t *p = (uint8_t *)eez::alloc(100, 0x1234);
    REQUIRE(p != nullptr);
    memset(p, 0x5A, 100);
    eez::initAllocHeap(nullptr, 0);
    CHECK_EQ((int)p[99], 0x5A);
    eez::free(p);
}

////////////////////////////////////////////////////////////////////////////////
// alloc (CM5: malloc)

TEST("core/alloc: alloc and free") {
    uint8_t *p = (uint8_t *)eez::alloc(100, 0x1234);
    REQUIRE(p != nullptr);
    memset(p, 0x5A, 100);
    CHECK_EQ((int)p[99], 0x5A);
    eez::free(p);
}

TEST("core/alloc: free of nullptr does nothing") {
    eez::free(nullptr);
    CHECK(true);
}

TEST("core/alloc: alloc of 0 bytes returns a pointer on CM5") {
    // malloc(0); the simulator allocator returns nullptr (CP1)
    void *p = eez::alloc(0, 0);
    CHECK(p != nullptr);
    eez::free(p);
}

TEST("core/alloc: many allocations are independent") {
    const int N = 1000;
    static uint8_t *blocks[N];
    for (int i = 0; i < N; i++) {
        blocks[i] = (uint8_t *)eez::alloc(16 + i % 64, i);
        REQUIRE(blocks[i] != nullptr);
        memset(blocks[i], i & 0xFF, 16 + i % 64);
    }
    bool ok = true;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 16 + i % 64; j++) {
            if (blocks[i][j] != (i & 0xFF)) {
                ok = false;
            }
        }
        eez::free(blocks[i]);
    }
    CHECK(ok);
}

namespace {

int g_constructed;
int g_destructed;

struct Counted {
    int value = 42;
    Counted() { g_constructed++; }
    ~Counted() { g_destructed++; }
};

} // namespace

TEST("core/alloc: ObjectAllocator constructs and destroys") {
    Counted *obj = ObjectAllocator<Counted>::allocate(0x55);
    REQUIRE(obj != nullptr);
    CHECK_EQ(g_constructed, 1);
    CHECK_EQ(obj->value, 42);
    ObjectAllocator<Counted>::deallocate(obj);
    CHECK_EQ(g_destructed, 1);
    ObjectAllocator<Counted>::deallocate(nullptr);
    CHECK_EQ(g_destructed, 1);
}

TEST("core/alloc: getAllocInfo reports the system memory on CM5") {
    uint32_t freeMemory = 0, allocMemory = 0;
    getAllocInfo(freeMemory, allocMemory);
    // free includes MemAvailable of /proc/meminfo
    CHECK(freeMemory > 0);
}

TEST("core/alloc: string refs are freed with their last value") {
    // AddressSanitizer reports a double free or a use after free here
    for (int i = 0; i < 100; i++) {
        Value a = Value::makeStringRef("abc", -1, i);
        Value b = a;
        Value c = Value::concatenateString(a, b);
        a = c;
        CHECK_STR(a.getString(), "abcabc");
    }
}
