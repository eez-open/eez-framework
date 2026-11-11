/*
 * eez-framework tests: core/util (strings, numbers, IP/MAC addresses, time zones,
 * file names, easing, Interval, MovingAverage) and the UTF-8 helpers
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <eez/conf-internal.h>
#include <eez/core/util.h>
#include <eez/core/utf8.h>

#include "harness.h"
#include "test.h"

using namespace eez;

////////////////////////////////////////////////////////////////////////////////
// string copy and append

TEST("core/util: stringCopy copies and terminates") {
    char buffer[16];
    stringCopy(buffer, sizeof(buffer), "abc");
    CHECK_STR(buffer, "abc");
}

TEST("core/util: stringCopy cuts to the buffer size") {
    char buffer[8];
    memset(buffer, 'x', sizeof(buffer));
    stringCopy(buffer, 4, "abcdef");
    CHECK_STR(buffer, "abc");
    CHECK_EQ(buffer[4], 'x');
}

TEST("core/util: stringCopyLength copies a prefix") {
    char buffer[16];
    stringCopyLength(buffer, sizeof(buffer), "abcdef", 3);
    CHECK_STR(buffer, "abc");
    stringCopyLength(buffer, sizeof(buffer), "ab", 0);
    CHECK_STR(buffer, "");
}

TEST("core/util: stringAppendString") {
    char buffer[16] = "ab";
    stringAppendString(buffer, sizeof(buffer), "cd");
    CHECK_STR(buffer, "abcd");
}

TEST("core/util: stringAppendString cuts to the buffer size") {
    char buffer[8];
    memset(buffer, 'x', sizeof(buffer));
    strcpy(buffer, "ab");
    stringAppendString(buffer, 5, "cdef");
    CHECK_STR(buffer, "abcd");
    CHECK_EQ(buffer[5], 'x');
    // a full buffer stays as it is
    stringAppendString(buffer, 5, "z");
    CHECK_STR(buffer, "abcd");
}

TEST("core/util: stringAppendStringLength") {
    char buffer[16] = "ab";
    stringAppendStringLength(buffer, sizeof(buffer), "cdef", 2);
    CHECK_STR(buffer, "abcd");
    char small[5] = "ab";
    stringAppendStringLength(small, sizeof(small), "cdef", 4);
    CHECK_STR(small, "abcd");
}

TEST("core/util: stringAppend of integers") {
    char buffer[64] = "x=";
    stringAppendInt(buffer, sizeof(buffer), -5);
    CHECK_STR(buffer, "x=-5");
    buffer[0] = 0;
    stringAppendUInt32(buffer, sizeof(buffer), 4000000000u);
    CHECK_STR(buffer, "4000000000");
    buffer[0] = 0;
    stringAppendInt64(buffer, sizeof(buffer), -9000000000LL);
    CHECK_STR(buffer, "-9000000000");
    buffer[0] = 0;
    stringAppendUInt64(buffer, sizeof(buffer), 18446744073709551615ULL);
    CHECK_STR(buffer, "18446744073709551615");
}

TEST("core/util: stringAppendInt cuts to the buffer size") {
    char buffer[8];
    memset(buffer, 'x', sizeof(buffer));
    strcpy(buffer, "ab");
    stringAppendInt(buffer, 5, 12345);
    CHECK_STR(buffer, "ab12");
    CHECK_EQ(buffer[5], 'x');
}

TEST("core/util: stringAppend of floating point") {
    char buffer[64] = "";
    stringAppendFloat(buffer, sizeof(buffer), 2.5f);
    CHECK_STR(buffer, "2.5");
    buffer[0] = 0;
    stringAppendFloat(buffer, sizeof(buffer), 2.5f, 2);
    CHECK_STR(buffer, "2.50");
    buffer[0] = 0;
    stringAppendDouble(buffer, sizeof(buffer), 0.1);
    CHECK_STR(buffer, "0.1");
    buffer[0] = 0;
    stringAppendDouble(buffer, sizeof(buffer), 0.1, 3);
    CHECK_STR(buffer, "0.100");
    buffer[0] = 0;
    // %g: 6 significant digits
    stringAppendDouble(buffer, sizeof(buffer), 1234567.0);
    CHECK_STR(buffer, "1.23457e+06");
}

TEST("core/util: stringAppend of quantities") {
    char buffer[64] = "";
    stringAppendVoltage(buffer, sizeof(buffer), 5.0f);
    CHECK_STR(buffer, "5 V");
    buffer[0] = 0;
    stringAppendCurrent(buffer, sizeof(buffer), 0.5f);
    CHECK_STR(buffer, "0.5 A");
    buffer[0] = 0;
    stringAppendPower(buffer, sizeof(buffer), 10.0f);
    CHECK_STR(buffer, "10 W");
    buffer[0] = 0;
    stringAppendDuration(buffer, sizeof(buffer), 0.5f);
    CHECK_STR(buffer, "0.5 s");
    buffer[0] = 0;
    stringAppendDuration(buffer, sizeof(buffer), 0.05f);
    CHECK_STR(buffer, "50 ms");
    buffer[0] = 0;
    stringAppendLoad(buffer, sizeof(buffer), 470.0f);
    CHECK_STR(buffer, "470 ohm");
    buffer[0] = 0;
    stringAppendLoad(buffer, sizeof(buffer), 4700.0f);
    CHECK_STR(buffer, "4.7 Kohm");
    buffer[0] = 0;
    stringAppendLoad(buffer, sizeof(buffer), 2000000.0f);
    CHECK_STR(buffer, "2 Mohm");
}

////////////////////////////////////////////////////////////////////////////////
// string helpers

TEST("core/util: replaceCharacter") {
    char buffer[] = "a/b/c";
    replaceCharacter(buffer, '/', '\\');
    CHECK_STR(buffer, "a\\b\\c");
}

TEST("core/util: strcicmp and strncicmp") {
    CHECK_EQ(strcicmp("Hello", "hELLO"), 0);
    CHECK(strcicmp("abc", "abd") < 0);
    CHECK(strcicmp("abd", "ABC") > 0);
    CHECK(strcicmp("ab", "abc") < 0);
    CHECK_EQ(strncicmp("HELLO world", "hello there", 5), 0);
    CHECK(strncicmp("HELLO world", "hello there", 7) != 0);
    CHECK_EQ(strncicmp("abc", "xyz", 0), 0);
}

TEST("core/util: isStringEmpty") {
    CHECK(isStringEmpty(""));
    CHECK(isStringEmpty(" \t\r\n"));
    CHECK(!isStringEmpty("  a "));
}

TEST("core/util: startsWith and endsWith") {
    CHECK(startsWith("prefix.txt", "prefix"));
    CHECK(!startsWith("prefix.txt", "Prefix"));
    CHECK(startsWith("abc", ""));
    CHECK(!startsWith("ab", "abc"));
    CHECK(!startsWith(nullptr, "a"));
    CHECK(!startsWith("a", nullptr));
    CHECK(startsWithNoCase("PREFIX.txt", "prefix"));
    CHECK(!startsWithNoCase(nullptr, "a"));

    CHECK(endsWith("file.TXT", ".TXT"));
    CHECK(!endsWith("file.TXT", ".txt"));
    CHECK(!endsWith("t", ".txt"));
    CHECK(!endsWith(nullptr, ".txt"));
    CHECK(endsWithNoCase("file.TXT", ".txt"));
    CHECK(!endsWithNoCase("file.TXT", ".csv"));
}

TEST("core/util: character classes and hex digits") {
    CHECK(isDigit('0') && isDigit('9') && !isDigit('a'));
    CHECK(isHexDigit('a') && isHexDigit('F') && isHexDigit('7') && !isHexDigit('g'));
    CHECK(isUperCaseLetter('Q') && !isUperCaseLetter('q'));
    CHECK_EQ(toHexDigit(0), '0');
    CHECK_EQ(toHexDigit(9), '9');
    CHECK_EQ(toHexDigit(11), 'B');
    CHECK_EQ(fromHexDigit('7'), 7);
    CHECK_EQ(fromHexDigit('b'), 11);
    CHECK_EQ(fromHexDigit('F'), 15);
}

TEST("core/util: formatBytes") {
    char text[32];
    formatBytes(0, text, sizeof(text));
    CHECK_STR(text, "0 Bytes");
    formatBytes(1, text, sizeof(text));
    CHECK_STR(text, "1 Bytes");
    formatBytes(1024, text, sizeof(text));
    CHECK_STR(text, "1 KB");
    formatBytes(1536, text, sizeof(text));
    CHECK_STR(text, "1.5 KB");
    formatBytes(1234567, text, sizeof(text));
    CHECK_STR(text, "1.18 MB");
    formatBytes(5ULL * 1024 * 1024 * 1024 * 1024, text, sizeof(text));
    CHECK_STR(text, "5 TB");
}

TEST("core/util: getFileName and getBaseFileName") {
    char name[32];
    getFileName("/a/b/c.txt", name, sizeof(name));
    CHECK_STR(name, "c.txt");
    getFileName("plain", name, sizeof(name));
    CHECK_STR(name, "plain");
    getFileName("/dir/", name, sizeof(name));
    CHECK_STR(name, "");
    getFileName("/a/longname.txt", name, 5);
    CHECK_STR(name, "long");

    getBaseFileName("/a/b/c.txt", name, sizeof(name));
    CHECK_STR(name, "c");
    getBaseFileName("/a.b/c", name, sizeof(name));
    CHECK_STR(name, "c");
    getBaseFileName("archive.tar.gz", name, sizeof(name));
    CHECK_STR(name, "archive.tar");
    getBaseFileName("/a/longname.txt", name, 5);
    CHECK_STR(name, "long");
}

TEST("core/util: getParentDir") {
    char dir[32];
    getParentDir("/a/b/c.txt", dir);
    CHECK_STR(dir, "/a/b");
    getParentDir("/file", dir);
    CHECK_STR(dir, "");
    getParentDir("file", dir);
    CHECK_STR(dir, "");
}

////////////////////////////////////////////////////////////////////////////////
// numbers

TEST("core/util: crc32") {
    CHECK_EQ(crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
    CHECK_EQ(crc32((const uint8_t *)"", 0), 0u);
}

TEST("core/util: BCD") {
    CHECK_EQ((int)toBCD(45), 0x45);
    CHECK_EQ((int)toBCD(9), 0x09);
    CHECK_EQ((int)fromBCD(0x45), 45);
    CHECK_EQ((int)fromBCD(0x99), 99);
}

TEST("core/util: roundPrec, floorPrec, ceilPrec") {
    CHECK_NEAR(roundPrec(1.234f, 0.01f), 1.23, 1e-6);
    CHECK_NEAR(roundPrec(1.236f, 0.01f), 1.24, 1e-6);
    CHECK_NEAR(floorPrec(1.239f, 0.01f), 1.23, 1e-6);
    CHECK_NEAR(ceilPrec(1.231f, 0.01f), 1.24, 1e-6);
    CHECK_NEAR(roundPrec(1234.0f, 100.0f), 1200.0, 1e-3);
}

TEST("core/util: isNaN") {
    CHECK(isNaN(NAN));
    CHECK(isNaN((double)NAN));
    CHECK(!isNaN(0.0f));
    CHECK(!isNaN(INFINITY));
}

TEST("core/util: remap and clamp") {
    CHECK_NEAR(remap(5.0f, 0.0f, 0.0f, 10.0f, 100.0f), 50.0, 1e-5);
    CHECK_NEAR(remap(0.0f, 0.0f, 10.0f, 10.0f, 20.0f), 10.0, 1e-5);
    CHECK_NEAR(remap(15.0f, 0.0f, 0.0f, 10.0f, 100.0f), 150.0, 1e-5);
    CHECK_NEAR(remapQuad(5.0f, 0.0f, 0.0f, 10.0f, 100.0f), 25.0, 1e-4);
    CHECK_NEAR(remapOutQuad(5.0f, 0.0f, 0.0f, 10.0f, 100.0f), 75.0, 1e-4);
    CHECK_NEAR(remapCubic(10.0f, 0.0f, 0.0f, 10.0f, 100.0f), 100.0, 1e-4);
    CHECK_NEAR(clamp(5.0f, 0.0f, 1.0f), 1.0, 0);
    CHECK_NEAR(clamp(-5.0f, 0.0f, 1.0f), 0.0, 0);
    CHECK_NEAR(clamp(0.5f, 0.0f, 1.0f), 0.5, 0);
}

TEST("core/util: pointInsideRect excludes the right and bottom edges") {
    CHECK(pointInsideRect(10, 10, 10, 10, 5, 5));
    CHECK(pointInsideRect(14, 14, 10, 10, 5, 5));
    CHECK(!pointInsideRect(15, 10, 10, 10, 5, 5));
    CHECK(!pointInsideRect(10, 15, 10, 10, 5, 5));
    CHECK(!pointInsideRect(9, 10, 10, 10, 5, 5));
}

TEST("core/util: easing functions start at 0 and end at 1") {
    // linear .. easeInOutBounce, the order of the Studio easing list
    for (int i = 0; i < 31; i++) {
        float start = g_easingFuncs[i](0.0f);
        float end = g_easingFuncs[i](1.0f);
        if (fabsf(start) > 1e-3f || fabsf(end - 1.0f) > 1e-3f) {
            test::fail(__FILE__, __LINE__, test::format("easing %d: f(0) = %g, f(1) = %g", i, start, end));
        }
    }
    CHECK_NEAR(g_easingFuncs[0](0.25f), 0.25, 1e-6);
    CHECK_NEAR(g_easingFuncs[1](0.5f), 0.25, 1e-6);
}

////////////////////////////////////////////////////////////////////////////////
// MAC and IP addresses

TEST("core/util: parseMacAddress") {
    uint8_t mac[6];
    CHECK(parseMacAddress("00-1A-2b-3C-4D-5E", 17, mac));
    CHECK(memcmp(mac, "\x00\x1A\x2B\x3C\x4D\x5E", 6) == 0);
    CHECK(parseMacAddress("001A2B 3C4D5E", 13, mac));
    CHECK(memcmp(mac, "\x00\x1A\x2B\x3C\x4D\x5E", 6) == 0);
}

TEST("core/util: parseMacAddress rejects bad input") {
    uint8_t mac[6] = { 1, 1, 1, 1, 1, 1 };
    CHECK(!parseMacAddress("00-1A-2B-3C-4D", 14, mac));
    CHECK(!parseMacAddress("00-1A-2B-3C-4D-5E-6F", 20, mac));
    CHECK(!parseMacAddress("00-1A-2B-3C-4D-5", 16, mac));
    CHECK(!parseMacAddress("00:1A:2B:3C:4D:5E", 17, mac));
    CHECK(!parseMacAddress("G0-1A-2B-3C-4D-5E", 17, mac));
    // the output is untouched on failure
    CHECK(memcmp(mac, "\x01\x01\x01\x01\x01\x01", 6) == 0);
}

TEST("core/util: macAddressToString") {
    uint8_t mac[6] = { 0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E };
    char text[18];
    macAddressToString(mac, text);
    CHECK_STR(text, "00-1A-2B-3C-4D-5E");
}

TEST("core/util: IP address parts") {
    uint32_t ip = getIpAddress(192, 168, 1, 2);
    CHECK_EQ(getIpAddressPartA(ip), 192);
    CHECK_EQ(getIpAddressPartB(ip), 168);
    CHECK_EQ(getIpAddressPartC(ip), 1);
    CHECK_EQ(getIpAddressPartD(ip), 2);
    setIpAddressPartC(&ip, 77);
    CHECK_EQ(getIpAddressPartC(ip), 77);

    uint8_t parts[4];
    ipAddressToArray(ip, parts);
    CHECK(memcmp(parts, "\xC0\xA8\x4D\x02", 4) == 0);
    CHECK_EQ(arrayToIpAddress(parts), ip);

    char text[16];
    ipAddressToString(ip, text, sizeof(text));
    CHECK_STR(text, "192.168.77.2");
}

TEST("core/util: parseIpAddress") {
    uint32_t ip = 0;
    CHECK(parseIpAddress("192.168.1.2", 11, ip));
    CHECK_EQ(ip, getIpAddress(192, 168, 1, 2));
    CHECK(parseIpAddress("0.0.0.0", 7, ip));
    CHECK_EQ(ip, 0u);
    CHECK(parseIpAddress("255.255.255.255", 15, ip));
    CHECK_EQ(ip, 0xFFFFFFFFu);
    // only the given length is parsed
    CHECK(parseIpAddress("10.0.0.1 trailing", 8, ip));
    CHECK_EQ(ip, getIpAddress(10, 0, 0, 1));
}

TEST("core/util: parseIpAddress rejects bad input") {
    uint32_t ip = 12345;
    CHECK(!parseIpAddress("256.1.1.1", 9, ip));
    CHECK(!parseIpAddress("1.2.3", 5, ip));
    CHECK(!parseIpAddress("1.2.3.4.5", 9, ip));
    CHECK(!parseIpAddress("1..2.3", 6, ip));
    CHECK(!parseIpAddress("a.b.c.d", 7, ip));
    CHECK(!parseIpAddress("", 0, ip));
    CHECK(!parseIpAddress("1.2.3.4444", 10, ip));
    CHECK_EQ(ip, 12345u);
}

TEST_BUG("core/util: parseIpAddress reads only the given length", "CK3@san") {
    // after a three-digit octet it reads the next character to look for '.',
    // also past the end: the input is a buffer of exactly 3 characters
    char *buffer = (char *)malloc(3);
    memcpy(buffer, "192", 3);
    uint32_t ip;
    CHECK(!parseIpAddress(buffer, 3, ip));
    ::free(buffer);
}

////////////////////////////////////////////////////////////////////////////////
// time zones

TEST("core/util: formatTimeZone") {
    char text[32];
    formatTimeZone(0, text, sizeof(text));
    CHECK_STR(text, "GMT");
    formatTimeZone(100, text, sizeof(text));
    CHECK_STR(text, "+01:00 GMT");
    formatTimeZone(-530, text, sizeof(text));
    CHECK_STR(text, "-05:30 GMT");
    formatTimeZone(1245, text, sizeof(text));
    CHECK_STR(text, "+12:45 GMT");
}

TEST("core/util: parseTimeZone") {
    int16_t tz = 0;
    CHECK(parseTimeZone("+01:00", 6, tz));
    CHECK_EQ((int)tz, 100);
    CHECK(parseTimeZone("-5:30", 5, tz));
    CHECK_EQ((int)tz, -530);
    CHECK(parseTimeZone("2", 1, tz));
    CHECK_EQ((int)tz, 200);
    CHECK(parseTimeZone(" + 14 ", 6, tz));
    CHECK_EQ((int)tz, 1400);
    CHECK(parseTimeZone("-12:00", 6, tz));
    CHECK_EQ((int)tz, -1200);
}

TEST("core/util: parseTimeZone rejects bad input") {
    int16_t tz = 77;
    CHECK(!parseTimeZone("+15", 3, tz));
    CHECK(!parseTimeZone("-13", 3, tz));
    CHECK(!parseTimeZone("+1:3", 4, tz));
    CHECK(!parseTimeZone("+1:", 3, tz));
    CHECK(!parseTimeZone("abc", 3, tz));
    CHECK(!parseTimeZone("", 0, tz));
    CHECK(!parseTimeZone("+123", 4, tz));
    CHECK_EQ((int)tz, 77);
}

TEST_BUG("core/util: parseTimeZone rejects minutes over 59", "CK3") {
    int16_t tz = 0;
    CHECK(!parseTimeZone("+01:75", 6, tz));
}

////////////////////////////////////////////////////////////////////////////////
// Interval, MovingAverage

TEST("core/util: Interval") {
    Interval interval;
    CHECK(interval.test(100));
    harness::advance(50);
    CHECK(!interval.test(100));
    harness::advance(50);
    CHECK(interval.test(100));
    CHECK(!interval.test(100));
    // after a long pause it fires once, on the interval grid
    harness::advance(350);
    CHECK(interval.test(100));
    harness::advance(50);
    CHECK(interval.test(100));
}

TEST_BUG("core/util: Interval waits across the millis() wrap", "C13") {
    // lastTime + interval wraps to a small number, so the test passes at once
    harness::advance(0xFFFFFFFFu - harness::now() - 20);
    Interval interval;
    CHECK(interval.test(100));
    harness::advance(10);
    CHECK(!interval.test(100));
}

TEST("core/util: MovingAverage") {
    MovingAverage<int, int, 3> average;
    average(3);
    CHECK_EQ((int)average, 3);
    average(6);
    CHECK_EQ((int)average, 4);
    average(9);
    CHECK_EQ((int)average, 6);
    // window of 3: the oldest sample (3) drops out
    average(12);
    CHECK_EQ((int)average, 9);
    average.reset();
    average(1);
    CHECK_EQ((int)average, 1);
}

TEST_BUG("core/util: MovingAverage of uint32 samples with a uint64 total", "CK2") {
    // sample - oldest wraps in uint32 when the new sample is smaller
    MovingAverage<uint32_t, uint64_t, 2> average;
    average(10);
    average(10);
    average(4);
    CHECK_EQ((uint32_t)average, 7u);
}

////////////////////////////////////////////////////////////////////////////////
// UTF-8 (UTF8_SUPPORT 1: eez/libs/utf8.h)

TEST("core/utf8: length and code points") {
    const char *str = "a\xC2\xB4" "b"; // "a", U+00B4, "b"
    CHECK_EQ((int)utf8len(str), 3);
    utf8_int32_t cp;
    const char *p = utf8codepoint(str, &cp);
    CHECK_EQ((int)cp, 'a');
    p = utf8codepoint(p, &cp);
    CHECK_EQ((int)cp, 0xB4);
    p = utf8codepoint(p, &cp);
    CHECK_EQ((int)cp, 'b');
    CHECK_EQ(*p, 0);
}

TEST("core/utf8: encode a code point") {
    char buffer[8] = { 0 };
    char *end = utf8catcodepoint(buffer, 0x20AC, sizeof(buffer));
    REQUIRE(end != nullptr);
    CHECK_EQ((int)(end - buffer), 3);
    CHECK(memcmp(buffer, "\xE2\x82\xAC", 3) == 0);
    // no room
    CHECK(utf8catcodepoint(buffer, 0x20AC, 2) == nullptr);
}

TEST("core/utf8: compare") {
    CHECK_EQ(utf8cmp("abc", "abc"), 0);
    CHECK(utf8cmp("abc", "abd") < 0);
    CHECK_EQ(utf8casecmp("ABC", "abc"), 0);
}
