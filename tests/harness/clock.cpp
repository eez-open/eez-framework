/*
 * eez-framework tests: the fake clock.
 *
 * The test executable is linked with --wrap=_ZN3eez6millisEv, so every call of
 * eez::millis() from the framework comes here.
 */

#include <stdint.h>

#include "harness.h"

// start away from 0, the framework uses 0 as "not set" in some places
static uint32_t g_now = 1000;

extern "C" uint32_t __wrap__ZN3eez6millisEv() {
    return g_now;
}

namespace harness {

uint32_t now() {
    return g_now;
}

void advance(uint32_t ms) {
    g_now += ms;
}

} // namespace harness
