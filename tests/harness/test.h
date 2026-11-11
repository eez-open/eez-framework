/*
 * eez-framework tests: a small test runner.
 *
 *   TEST("area: what is tested") { CHECK(...); }
 *   TEST_BUG("area: what is tested", "G1") { ... }
 *
 * Every test runs in its own forked process, which starts from the state the
 * harness has after harness::init() (assets loaded, flow started, main page shown).
 * A crash, a hang (timeout) or a sanitizer report fails only that test.
 *
 * TEST_BUG marks a test of a known bug (the id of the review finding). It is
 * expected to fail: the runner reports XFAIL. When it passes (XPASS), the bug is
 * fixed and the marker has to be removed, so XPASS counts as a failure.
 *
 * A bug id with the suffix "@san" ("C14@san") is a bug that only
 * AddressSanitizer / UndefinedBehaviorSanitizer show (a read past a buffer, a use
 * after free that happens to work): in a build without them the test passes and
 * is still counted as a known bug.
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <string>
#include <functional>

namespace test {

struct Abort {};

void registerTest(const std::string &name, std::function<void()> func, const char *knownBug = nullptr);

void fail(const char *file, int line, const std::string &message);

std::string format(const char *fmt, ...);

inline std::string str(bool v) { return v ? "true" : "false"; }
inline std::string str(int v) { return std::to_string(v); }
inline std::string str(unsigned v) { return std::to_string(v); }
inline std::string str(long v) { return std::to_string(v); }
inline std::string str(unsigned long v) { return std::to_string(v); }
inline std::string str(long long v) { return std::to_string(v); }
inline std::string str(unsigned long long v) { return std::to_string(v); }
inline std::string str(double v) { return format("%.17g", v); }
inline std::string str(float v) { return format("%.9g", (double)v); }
inline std::string str(const char *v) { return v ? format("\"%s\"", v) : "nullptr"; }
inline std::string str(const std::string &v) { return format("\"%s\"", v.c_str()); }
template <typename T> std::string str(T *v) { return format("%p", (const void *)v); }

struct Registrar {
    Registrar(const char *name, void (*func)(), const char *knownBug) {
        registerTest(name, func, knownBug);
    }
};

} // namespace test

#define TEST_CAT2(A, B) A##B
#define TEST_CAT(A, B) TEST_CAT2(A, B)

#define TEST_IMPL(NAME, BUG, FUNC) \
    static void FUNC(); \
    static test::Registrar TEST_CAT(FUNC, _registrar)(NAME, FUNC, BUG); \
    static void FUNC()

#define TEST(NAME) TEST_IMPL(NAME, nullptr, TEST_CAT(test_func_, __LINE__))
#define TEST_BUG(NAME, BUG) TEST_IMPL(NAME, BUG, TEST_CAT(test_func_, __LINE__))

#define CHECK(COND) \
    do { if (!(COND)) test::fail(__FILE__, __LINE__, "CHECK(" #COND ")"); } while (0)

#define REQUIRE(COND) \
    do { if (!(COND)) { test::fail(__FILE__, __LINE__, "REQUIRE(" #COND ")"); throw test::Abort(); } } while (0)

#define CHECK_EQ(A, B) \
    do { \
        auto test_a_ = (A); \
        auto test_b_ = (B); \
        if (!(test_a_ == test_b_)) { \
            test::fail(__FILE__, __LINE__, std::string("CHECK_EQ(" #A ", " #B "): ") + \
                test::str(test_a_) + " != " + test::str(test_b_)); \
        } \
    } while (0)

#define CHECK_STR(A, B) \
    do { \
        const char *test_a_ = (A); \
        const char *test_b_ = (B); \
        if (!test_a_ || !test_b_ || strcmp(test_a_, test_b_) != 0) { \
            test::fail(__FILE__, __LINE__, std::string("CHECK_STR(" #A ", " #B "): ") + \
                test::str(test_a_) + " != " + test::str(test_b_)); \
        } \
    } while (0)

#define CHECK_NEAR(A, B, EPS) \
    do { \
        double test_a_ = (A); \
        double test_b_ = (B); \
        if (!(test_a_ - test_b_ <= (EPS) && test_b_ - test_a_ <= (EPS))) { \
            test::fail(__FILE__, __LINE__, std::string("CHECK_NEAR(" #A ", " #B "): ") + \
                test::str(test_a_) + " != " + test::str(test_b_)); \
        } \
    } while (0)
