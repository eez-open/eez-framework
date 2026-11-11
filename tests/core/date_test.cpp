/*
 * eez-framework tests: flow/date through its C++ API (makeDate, breakDate,
 * toString, fromString, toLocaleString, time zone and DST). The Date.*
 * expression functions are tested with the expressions.
 */

#include <string.h>
#include <unistd.h>

#include <string>

#include <eez/conf-internal.h>
#include <eez/flow/date.h>
#include <eez/flow/hooks.h>

#include "test.h"

using namespace eez::flow;
using namespace eez::flow::date;

namespace {

const Date MS_PER_DAY = 24ULL * 3600 * 1000;
const Date MS_PER_HOUR = 3600ULL * 1000;

// 2024-02-29T12:34:56.789Z
const Date LEAP_DAY = 1709210096789ULL;

struct Parts {
    int year, month, day, hours, minutes, seconds, milliseconds;
};

Parts parts(Date date) {
    Parts p;
    breakDate(date, p.year, p.month, p.day, p.hours, p.minutes, p.seconds, p.milliseconds);
    return p;
}

std::string iso(Date date) {
    char buffer[64];
    toString(date, buffer, sizeof(buffer));
    return buffer;
}

std::string locale(Date date) {
    char buffer[64];
    toLocaleString(date, buffer, sizeof(buffer));
    return buffer;
}

void setZone(int timeZone, DstRule dstRule) {
    g_timeZone = timeZone;
    g_dstRule = dstRule;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// makeDate, breakDate

TEST("core/date: makeDate of the epoch is 0") {
    CHECK_EQ(makeDate(1970, 1, 1, 0, 0, 0, 0), (Date)0);
}

TEST("core/date: makeDate is milliseconds since the epoch, UTC") {
    CHECK_EQ(makeDate(2000, 1, 1, 0, 0, 0, 0), (Date)946684800000ULL);
    CHECK_EQ(makeDate(2024, 2, 29, 12, 34, 56, 789), LEAP_DAY);
}

TEST("core/date: leap years") {
    // 2000 is a leap year (divisible by 400), 2100 is not
    CHECK_EQ(makeDate(2000, 3, 1, 0, 0, 0, 0) - makeDate(2000, 2, 28, 0, 0, 0, 0), 2 * MS_PER_DAY);
    CHECK_EQ(makeDate(2100, 3, 1, 0, 0, 0, 0) - makeDate(2100, 2, 28, 0, 0, 0, 0), MS_PER_DAY);
    CHECK_EQ(makeDate(2024, 3, 1, 0, 0, 0, 0) - makeDate(2024, 2, 28, 0, 0, 0, 0), 2 * MS_PER_DAY);
    CHECK_EQ(makeDate(2023, 3, 1, 0, 0, 0, 0) - makeDate(2023, 2, 28, 0, 0, 0, 0), MS_PER_DAY);
    CHECK_EQ(makeDate(2025, 1, 1, 0, 0, 0, 0) - makeDate(2024, 1, 1, 0, 0, 0, 0), 366 * MS_PER_DAY);
}

TEST("core/date: makeDate accepts hours, minutes and seconds past their range") {
    // pinned: they are added as they are (like JavaScript Date.UTC)
    CHECK_EQ(makeDate(2024, 1, 1, 25, 0, 0, 0), makeDate(2024, 1, 2, 1, 0, 0, 0));
    CHECK_EQ(makeDate(2024, 1, 1, 0, 90, 0, 0), makeDate(2024, 1, 1, 1, 30, 0, 0));
    CHECK_EQ(makeDate(2024, 1, 1, 0, 0, 0, 1500), makeDate(2024, 1, 1, 0, 0, 1, 500));
}

TEST("core/date: breakDate") {
    Parts p = parts(LEAP_DAY);
    CHECK_EQ(p.year, 2024);
    CHECK_EQ(p.month, 2);
    CHECK_EQ(p.day, 29);
    CHECK_EQ(p.hours, 12);
    CHECK_EQ(p.minutes, 34);
    CHECK_EQ(p.seconds, 56);
    CHECK_EQ(p.milliseconds, 789);
}

TEST("core/date: breakDate of the epoch and of year ends") {
    Parts p = parts(0);
    CHECK_EQ(p.year, 1970);
    CHECK_EQ(p.month, 1);
    CHECK_EQ(p.day, 1);
    CHECK_EQ(p.hours, 0);

    p = parts(makeDate(2024, 12, 31, 23, 59, 59, 999));
    CHECK_EQ(p.year, 2024);
    CHECK_EQ(p.month, 12);
    CHECK_EQ(p.day, 31);
    CHECK_EQ(p.milliseconds, 999);

    p = parts(makeDate(2024, 12, 31, 23, 59, 59, 999) + 1);
    CHECK_EQ(p.year, 2025);
    CHECK_EQ(p.month, 1);
    CHECK_EQ(p.day, 1);
    CHECK_EQ(p.hours, 0);
}

TEST("core/date: makeDate and breakDate round trip from 1970 to 2200") {
    int failures = 0;
    for (Date date = 0; date < makeDate(2200, 1, 1, 0, 0, 0, 0); date += 37 * MS_PER_DAY + 7 * MS_PER_HOUR + 123457) {
        Parts p = parts(date);
        Date back = makeDate(p.year, p.month, p.day, p.hours, p.minutes, p.seconds, p.milliseconds);
        if (back != date && failures++ < 5) {
            test::fail(__FILE__, __LINE__, test::format("%llu -> %04d-%02d-%02d -> %llu",
                (unsigned long long)date, p.year, p.month, p.day, (unsigned long long)back));
        }
    }
    CHECK_EQ(failures, 0);
}

TEST("core/date: getYear .. getMilliseconds") {
    CHECK_EQ(getYear(LEAP_DAY), 2024);
    CHECK_EQ(getMonth(LEAP_DAY), 2);
    CHECK_EQ(getDay(LEAP_DAY), 29);
    CHECK_EQ(getHours(LEAP_DAY), 12);
    CHECK_EQ(getMinutes(LEAP_DAY), 34);
    CHECK_EQ(getSeconds(LEAP_DAY), 56);
    CHECK_EQ(getMilliseconds(LEAP_DAY), 789);
}

TEST("core/date: a date before 1970") {
    // signed milliseconds, breakDate works out the date without loops (O8: it
    // counted days with a uint32 and never ended on LP64)
    alarm(2);
    CHECK_EQ(getYear(makeDate(1969, 12, 31, 0, 0, 0, 0)), 1969);
}

TEST("core/date: makeDate with month 13 is January of the next year") {
    // pinned: the 12 months of the year are added, monthDays is not read past
    // its end yet
    CHECK_EQ(makeDate(2024, 13, 1, 0, 0, 0, 0), makeDate(2025, 1, 1, 0, 0, 0, 0));
}

TEST("core/date: makeDate with a month over 13") {
    // the month is normalised like in JavaScript (O8: it read past the end of
    // monthDays)
    CHECK_EQ(makeDate(2024, 14, 1, 0, 0, 0, 0), makeDate(2025, 2, 1, 0, 0, 0, 0));
}

TEST("core/date: a date after 2225") {
    // the year is not limited (F20: breakDate kept the year offset in a uint8_t)
    CHECK_EQ(getYear(makeDate(2230, 6, 15, 8, 0, 0, 0)), 2230);
}

TEST("core/date: a date in 2225 still works") {
    CHECK_EQ(getYear(makeDate(2225, 12, 31, 0, 0, 0, 0)), 2225);
}

////////////////////////////////////////////////////////////////////////////////
// toString, fromString, toLocaleString

TEST("core/date: toString date and time part") {
    CHECK_EQ(iso(LEAP_DAY).substr(0, 20), std::string("2024-02-29T12:34:56."));
    CHECK_EQ(iso(0).substr(0, 20), std::string("1970-01-01T00:00:00."));
}

TEST("core/date: toString writes milliseconds with 3 digits") {
    // 789 ms is written as ".789" (O8: %06d wrote ".000789")
    CHECK_EQ(iso(LEAP_DAY), "2024-02-29T12:34:56.789");
}

TEST("core/date: fromString") {
    CHECK_EQ(fromString("2024-02-29T12:34:56.789"), LEAP_DAY);
    CHECK_EQ(fromString("2024-02-29"), makeDate(2024, 2, 29, 0, 0, 0, 0));
    CHECK_EQ(fromString("2024-02-29T12:34"), makeDate(2024, 2, 29, 12, 34, 0, 0));
}

TEST("core/date: fromString of toString is the same date") {
    CHECK_EQ(fromString(iso(LEAP_DAY).c_str()), LEAP_DAY);
    Date date = makeDate(2031, 7, 4, 1, 2, 3, 4);
    CHECK_EQ(fromString(iso(date).c_str()), date);
}

TEST_BUG("core/date: fromString reads the fraction as a fraction of a second", "NEW-core-2") {
    // "%d" reads the digits after the point as a number of milliseconds:
    // ".5" is 5 ms, not 500 ms (ISO 8601, JavaScript Date)
    CHECK_EQ(fromString("2024-02-29T12:34:56.5"), makeDate(2024, 2, 29, 12, 34, 56, 500));
}

TEST("core/date: toLocaleString formats") {
    g_localeFormat = FORMAT_DMY_24;
    CHECK_EQ(locale(LEAP_DAY), "29-02-2024 12:34:56.789");
    g_localeFormat = FORMAT_MDY_24;
    CHECK_EQ(locale(LEAP_DAY), "02-29-2024 12:34:56.789");
    g_localeFormat = FORMAT_DMY_12;
    CHECK_EQ(locale(LEAP_DAY), "29-02-2024 12:34:56.789 PM");
    g_localeFormat = FORMAT_MDY_12;
    CHECK_EQ(locale(LEAP_DAY), "02-29-2024 12:34:56.789 PM");
}

TEST("core/date: toLocaleString 12 hour clock at midnight and noon") {
    g_localeFormat = FORMAT_DMY_12;
    CHECK_EQ(locale(makeDate(2024, 1, 2, 0, 5, 0, 0)), "02-01-2024 12:05:00.000 AM");
    CHECK_EQ(locale(makeDate(2024, 1, 2, 12, 5, 0, 0)), "02-01-2024 12:05:00.000 PM");
    CHECK_EQ(locale(makeDate(2024, 1, 2, 9, 5, 0, 0)), "02-01-2024 09:05:00.000 AM");
    CHECK_EQ(locale(makeDate(2024, 1, 2, 23, 5, 0, 0)), "02-01-2024 11:05:00.000 PM");
}

TEST("core/date: toString is cut to the buffer") {
    char buffer[11];
    toString(LEAP_DAY, buffer, sizeof(buffer));
    CHECK_STR(buffer, "2024-02-29");
}

////////////////////////////////////////////////////////////////////////////////
// time zone and DST

TEST("core/date: utcToLocal and localToUtc with a time zone, no DST") {
    setZone(100, DST_RULE_OFF);
    CHECK_EQ(utcToLocal(LEAP_DAY), LEAP_DAY + MS_PER_HOUR);
    CHECK_EQ(localToUtc(LEAP_DAY + MS_PER_HOUR), LEAP_DAY);

    setZone(-530, DST_RULE_OFF);
    CHECK_EQ(utcToLocal(LEAP_DAY), LEAP_DAY - 5 * MS_PER_HOUR - 30 * 60 * 1000);
    CHECK_EQ(localToUtc(utcToLocal(LEAP_DAY)), LEAP_DAY);

    setZone(0, DST_RULE_OFF);
    CHECK_EQ(utcToLocal(LEAP_DAY), LEAP_DAY);
}

TEST("core/date: now() is the hook's UTC time in the local time zone") {
    getDateNowHook = [] { return (double)LEAP_DAY; };
    setZone(200, DST_RULE_OFF);
    CHECK_EQ(now(), LEAP_DAY + 2 * MS_PER_HOUR);
}

TEST("core/date: Europe DST in summer and winter") {
    setZone(100, DST_RULE_EUROPE);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 7, 1, 12, 0, 0, 0))), 14);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 1, 15, 12, 0, 0, 0))), 13);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 12, 1, 12, 0, 0, 0))), 13);
}

TEST("core/date: Australia DST in their summer and winter") {
    setZone(1000, DST_RULE_AUSTRALIA);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 1, 15, 0, 0, 0, 0))), 11);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 7, 1, 0, 0, 0, 0))), 10);
}

TEST("core/date: localToUtc in DST") {
    setZone(100, DST_RULE_EUROPE);
    Date utc = makeDate(2024, 7, 1, 12, 0, 0, 0);
    CHECK_EQ(localToUtc(utcToLocal(utc)), utc);
}

TEST("core/date: Europe DST starts on the last Sunday of March") {
    // 2024-03-31 01:00 UTC (O8: the day offset was added in seconds to
    // milliseconds, so DST started around April 1)
    setZone(100, DST_RULE_EUROPE);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 3, 31, 12, 0, 0, 0))), 14);
}

TEST("core/date: Europe DST ends on the last Sunday of October") {
    // 2024-10-27 01:00 UTC (O8: DST lasted until around November 1)
    setZone(100, DST_RULE_EUROPE);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 10, 29, 12, 0, 0, 0))), 13);
}

TEST("core/date: USA DST starts on the second Sunday of March") {
    // 2024-03-10 (F20: DST started a few minutes after March 1)
    setZone(-500, DST_RULE_USA);
    CHECK_EQ(getHours(utcToLocal(makeDate(2024, 3, 5, 12, 0, 0, 0))), 7);
}
