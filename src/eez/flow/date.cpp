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

#include <stdio.h>

#include <eez/core/os.h>

#include <eez/flow/date.h>
#include <eez/flow/hooks.h>

namespace eez {
namespace flow {
namespace date {

static const int64_t MS_PER_SECOND = 1000;
static const int64_t MS_PER_MINUTE = 60 * MS_PER_SECOND;
static const int64_t MS_PER_HOUR = 60 * MS_PER_MINUTE;
static const int64_t MS_PER_DAY = 24 * MS_PER_HOUR;

////////////////////////////////////////////////////////////////////////////////

enum Week { Last, First, Second, Third, Fourth };
enum DayOfWeek { Sun = 1, Mon, Tue, Wed, Thu, Fri, Sat };
enum Month { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

struct TimeChangeRule {
    Week week;
    DayOfWeek dow;
    uint8_t month;
    uint8_t hours;
};

static struct {
    TimeChangeRule dstStart;
    TimeChangeRule dstEnd;
} g_dstRules[] = {
    { { Last, Sun, Mar, 2 }, { Last, Sun, Oct, 3 } },    // EUROPE
    { { Second, Sun, Mar, 2 }, { First, Sun, Nov, 2 } }, // USA
    { { First, Sun, Oct, 2 }, { First, Sun, Apr, 3 } },  // Australia
};

////////////////////////////////////////////////////////////////////////////////

Format g_localeFormat = FORMAT_DMY_24;
int g_timeZone = 0;
DstRule g_dstRule = DST_RULE_OFF;

//
// PRIVATE function declarations
//

static int64_t floorDiv(int64_t a, int64_t b);
static int64_t daysFromCivil(int64_t year, int month, int day);
static void civilFromDays(int64_t days, int64_t &year, int &month, int &day);
static void convertTime24to12(int &hours, bool &am);
//static void convertTime12to24(int &hours, bool am);
static bool isDst(Date time, DstRule dstRule);
static uint8_t dayOfWeek(int y, int m, int d);
static Date timeChangeRuleToLocal(TimeChangeRule &r, int year);

////////////////////////////////////////////////////////////////////////////////

Date now() {
    return utcToLocal(getDateNowHook());
}

void toString(Date time, char *str, uint32_t strLen) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    snprintf(str, strLen, "%04d-%02d-%02dT%02d:%02d:%02d.%03d", year, month, day, hours, minutes, seconds, milliseconds);
}

void toLocaleString(Date time, char *str, uint32_t strLen) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    if (g_localeFormat == FORMAT_DMY_24) {
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d", day, month, year, hours, minutes, seconds, milliseconds);
    } else if (g_localeFormat == FORMAT_MDY_24) {
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d", month, day, year, hours, minutes, seconds, milliseconds);
    } else if (g_localeFormat == FORMAT_DMY_12) {
        bool am;
        convertTime24to12(hours, am);
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d %s", day, month, year, hours, minutes, seconds, milliseconds, am ? "AM" : "PM");
    } else if (g_localeFormat == FORMAT_MDY_12) {
        bool am;
        convertTime24to12(hours, am);
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d %s", month, day, year, hours, minutes, seconds, milliseconds, am ? "AM" : "PM");
    }
}

Date fromString(const char *str) {
    int year = 0, month = 0, day = 0, hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
    sscanf(str, "%d-%d-%dT%d:%d:%d.%d", &year, &month, &day, &hours, &minutes, &seconds, &milliseconds);
    return makeDate(year, month, day, hours, minutes, seconds, milliseconds);
}

Date makeDate(int year, int month, int day, int hours, int minutes, int seconds, int milliseconds) {
    // a month outside 1 to 12 continues into the previous or the next years
    // (month 0 is December of the previous year, month 13 January of the next
    // year), like JavaScript's Date
    int64_t monthIndex = (int64_t)month - 1;
    int64_t yearOffset = floorDiv(monthIndex, 12);
    monthIndex -= yearOffset * 12;

    // the day, hours, minutes, seconds and milliseconds are added as they are,
    // also when they are out of their range
    int64_t days = daysFromCivil(year + yearOffset, (int)monthIndex + 1, 1) + ((int64_t)day - 1);
    int64_t timeOfDay = hours * MS_PER_HOUR + minutes * MS_PER_MINUTE + seconds * MS_PER_SECOND + milliseconds;

    // unsigned math: a year far out of range wraps instead of overflowing
    return (Date)((uint64_t)days * (uint64_t)MS_PER_DAY + (uint64_t)timeOfDay);
}

void breakDate(Date time, int &result_year, int &result_month, int &result_day, int &result_hours, int &result_minutes, int &result_seconds, int &result_milliseconds) {
    // days since 1970-01-01 and milliseconds since the midnight of that day
    int64_t days = time / MS_PER_DAY;
    int64_t timeOfDay = time % MS_PER_DAY;
    if (timeOfDay < 0) {
        timeOfDay += MS_PER_DAY;
        days--;
    }

    result_milliseconds = (int)(timeOfDay % 1000);
    timeOfDay /= 1000; // now it is seconds

    result_seconds = (int)(timeOfDay % 60);
    timeOfDay /= 60; // now it is minutes

    result_minutes = (int)(timeOfDay % 60);
    result_hours = (int)(timeOfDay / 60);

    int64_t year;
    civilFromDays(days, year, result_month, result_day);
    result_year = (int)year;
}

int getYear(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return year;
}

int getMonth(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return month;
}

int getDay(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return day;
}

int getHours(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return hours;
}

int getMinutes(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return minutes;
}

int getSeconds(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return seconds;
}

int getMilliseconds(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return milliseconds;
}

Date utcToLocal(Date utc) {
    Date local = utc + ((g_timeZone / 100) * 60 + g_timeZone % 100) * MS_PER_MINUTE;
    if (isDst(local, g_dstRule)) {
        local += MS_PER_HOUR;
    }
    return local;
}

Date localToUtc(Date local) {
    Date utc = local - ((g_timeZone / 100) * 60 + g_timeZone % 100) * MS_PER_MINUTE;
    if (isDst(local, g_dstRule)) {
        utc -= MS_PER_HOUR;
    }
    return utc;
}

////////////////////////////////////////////////////////////////////////////////
//
// PRIVATE function definitions
//

// division rounded towards minus infinity, b > 0
static int64_t floorDiv(int64_t a, int64_t b) {
    int64_t quotient = a / b;
    if (a % b < 0) {
        quotient--;
    }
    return quotient;
}

// Days since 1970-01-01 of a date in the proleptic Gregorian calendar, month is
// 1 to 12, day 1 to 31 (days_from_civil of
// https://howardhinnant.github.io/date_algorithms.html).
static int64_t daysFromCivil(int64_t year, int month, int day) {
    // the year starts in March, so the leap day is the last day of the year
    if (month <= 2) {
        year--;
    }
    int64_t era = floorDiv(year, 400);
    int yearOfEra = (int)(year - era * 400);                                         // [0, 399]
    int dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;   // [0, 365]
    int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;    // [0, 146096]
    return era * 146097 + dayOfEra - 719468;
}

// The date of a number of days since 1970-01-01, month is 1 to 12 (civil_from_days
// of https://howardhinnant.github.io/date_algorithms.html).
static void civilFromDays(int64_t days, int64_t &year, int &month, int &day) {
    days += 719468; // days since 0000-03-01
    int64_t era = floorDiv(days, 146097);
    int dayOfEra = (int)(days - era * 146097);                                                   // [0, 146096]
    int yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;   // [0, 399]
    int dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);              // [0, 365]
    int monthFromMarch = (5 * dayOfYear + 2) / 153;                                              // [0, 11]
    day = dayOfYear - (153 * monthFromMarch + 2) / 5 + 1;
    month = monthFromMarch < 10 ? monthFromMarch + 3 : monthFromMarch - 9;
    year = era * 400 + yearOfEra + (month <= 2 ? 1 : 0);
}

static void convertTime24to12(int &hours, bool &am) {
    if (hours == 0) {
        hours = 12;
        am = true;
    } else if (hours < 12) {
        am = true;
    } else if (hours == 12) {
        am = false;
    } else {
        hours = hours - 12;
        am = false;
    }
}

//static void convertTime12to24(int &hours, bool am) {
//    if (am) {
//        if (hours == 12) {
//            hours = 0;
//        }
//    } else {
//        if (hours < 12) {
//            hours += 12;
//        }
//    }
//}

static bool isDst(Date local, DstRule dstRule) {
    if (dstRule == DST_RULE_OFF) {
        return false;
    }

    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(local, year, month, day, hours, minutes, seconds, milliseconds);

    Date dstStart = timeChangeRuleToLocal(g_dstRules[dstRule - 1].dstStart, year);
    Date dstEnd = timeChangeRuleToLocal(g_dstRules[dstRule - 1].dstEnd, year);

    return (dstStart < dstEnd && (local >= dstStart && local < dstEnd)) ||
           (dstStart > dstEnd && (local >= dstStart || local < dstEnd));
}

static uint8_t dayOfWeek(int y, int m, int d) {
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) {
        --y;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7 + 1;
}

static Date timeChangeRuleToLocal(TimeChangeRule &r, int year) {
    uint8_t month = r.month;
    uint8_t week = r.week;
    if (week == 0) {        // Last week == 0
        if (++month > 12) { // for "Last", go to the next month
            month = 1;
            ++year;
        }
        week = 1; // and treat as first week of next month, subtract 7 days later
    }

    Date time = makeDate(year, month, 1, r.hours, 0, 0, 0);

    uint8_t dow = dayOfWeek(year, month, 1);

    time += (7 * (week - 1) + (r.dow - dow + 7) % 7) * MS_PER_DAY;
    if (r.week == 0) {
        time -= 7 * MS_PER_DAY; // back up a week if this is a "Last" rule
    }

    return time;
}

} // namespace date
} // namespace flow
} // namespace eez
