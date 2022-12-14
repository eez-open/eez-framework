/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/conf-internal.h>

#include <assert.h>
#include <math.h>

#include <eez/flow/hooks.h>

#include <eez/core/util.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
#endif

#include <chrono>
#include <iostream>
#include <sstream>
#include <time.h>

#ifndef ARDUINO
// https://howardhinnant.github.io/date/date.html
#include <eez/libs/date.h>
#endif

namespace eez {
namespace flow {

static bool isFlowRunning() {
	return true;
}

static void replacePage(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay) {
#if EEZ_OPTION_GUI
	eez::gui::getAppContextFromId(APP_CONTEXT_ID_DEVICE)->replacePage(pageId);
#endif
}

static void showKeyboard(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void(*onOk)(char *), void(*onCancel)()) {
}

static void showKeypad(Value label, Value initialValue, Value min, Value max, Unit unit, void(*onOk)(float), void(*onCancel)()) {
}

static void stopScript() {
	assert(false);
}

static void scpiComponentInit() {
}

static void startToDebuggerMessage() {
}

static void writeDebuggerBuffer(const char *buffer, uint32_t length) {
}

static void finishToDebuggerMessage() {
}

static void onDebuggerInputAvailable() {
}

bool (*isFlowRunningHook)() = isFlowRunning;
void (*replacePageHook)(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay) = replacePage;
void (*showKeyboardHook)(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void(*onOk)(char *), void(*onCancel)()) = showKeyboard;
void (*showKeypadHook)(Value label, Value initialValue, Value min, Value max, Unit unit, void(*onOk)(float), void(*onCancel)()) = showKeypad;
void (*stopScriptHook)() = stopScript;

void (*scpiComponentInitHook)() = scpiComponentInit;

void (*startToDebuggerMessageHook)() = startToDebuggerMessage;
void (*writeDebuggerBufferHook)(const char *buffer, uint32_t length) = writeDebuggerBuffer;
void (*finishToDebuggerMessageHook)() = finishToDebuggerMessage;
void (*onDebuggerInputAvailableHook)() = onDebuggerInputAvailable;

void (*executeDashboardComponentHook)(uint16_t componentType, int flowStateIndex, int componentIndex) = nullptr;

void (*onArrayValueFreeHook)(ArrayValue *arrayValue) = nullptr;

#if defined(EEZ_FOR_LVGL)
static lv_obj_t *getLvglObjectFromIndex(int32_t index) {
    return 0;
}

static const void *getLvglImageByName(const char *name) {
    return 0;
}

lv_obj_t *(*getLvglObjectFromIndexHook)(int32_t index) = getLvglObjectFromIndex;
const void *(*getLvglImageByNameHook)(const char *name) = getLvglImageByName;
#endif

double getDatetimeNowDefaultImplementation() {
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    return (double)ms.count();
}

void formatDatetimeDefaultImplementation(double datetime,  char *str, size_t strLen) {
#ifndef ARDUINO
    using namespace std;
    using namespace std::chrono;
    using namespace date;

    auto tp = system_clock::time_point(milliseconds((long long)datetime));

    stringstream out;
    out << tp << endl;

    stringCopy(str, strLen, out.str().c_str());
#else
    // NOT IMPLEMENTED
    *str = 0;
#endif
}

extern double parseDatetimeDefaultImplementation(const char *str) {
#ifndef ARDUINO
    using namespace std;
    using namespace std::chrono;
    using namespace date;

    istringstream in{str};

    system_clock::time_point tp;
    in >> date::parse("%Y-%m-%d %T", tp);

    milliseconds ms = duration_cast<milliseconds>(tp.time_since_epoch());
    return (double)ms.count();
#else
    // NOT IMPLEMENTED
    return 0;
#endif
}

extern double makeDatetimeDefaultImplementation(int year, int month, int day, int hours, int minutes, int seconds) {
    tm ts;
    ts.tm_year = year - 1900;
    ts.tm_mon = month - 1;
    ts.tm_mday = day;
    ts.tm_hour = hours;
    ts.tm_min = minutes;
    ts.tm_sec = seconds;
    ts.tm_isdst = -1;
    return mktime(&ts) * 1000.0;
}

extern void breakDatetimeDefaultImplementation(double datetime, int *year, int *month, int *day, int *hours, int *minutes, int *seconds) {
    auto temp = (time_t)(datetime / 1000.0);
    tm ts = *gmtime(&temp);
    if (year) *year = ts.tm_year + 1900;
    if (month) *month = ts.tm_mon + 1;
    if (day) *day = ts.tm_mday;
    if (hours) *hours = ts.tm_hour;
    if (minutes) *minutes = ts.tm_min;
    if (seconds) *seconds = ts.tm_sec;
}

double (*getDatetimeNowHook)() = getDatetimeNowDefaultImplementation;
void (*formatDatetimeHook)(double datetime,  char *str, size_t strLen) = formatDatetimeDefaultImplementation;
double (*parseDatetimeHook)(const char *str) = parseDatetimeDefaultImplementation;
double (*makeDatetimeHook)(int year, int month, int day, int hours, int minutes, int seconds) = makeDatetimeDefaultImplementation;
void (*breakDatetimeHook)(double datetime, int *year, int *month, int *day, int *hours, int *minutes, int *seconds) = breakDatetimeDefaultImplementation;

} // namespace flow
} // namespace eez

