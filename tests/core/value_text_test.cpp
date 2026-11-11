/*
 * eez-framework tests: core/value text (toText, toString, type names)
 */

#include <math.h>
#include <string.h>

#include <string>

#include <eez/conf-internal.h>
#include <eez/core/value.h>
#include <eez/core/util.h>
#include <eez/flow/date.h>

#include "test.h"

using namespace eez;

namespace {

std::string text(const Value &value) {
    char buffer[128];
    value.toText(buffer, sizeof(buffer));
    return buffer;
}

std::string toStr(const Value &value) {
    Value s = value.toString(0x12345678);
    const char *str = s.getString();
    return str ? str : "<null>";
}

const char *typeName(const Value &value) {
    return g_valueTypeNames[value.type](value);
}

Value doubleWithUnit(double v, Unit unit, uint16_t options = 0) {
    Value value(v, VALUE_TYPE_DOUBLE);
    value.unit = unit;
    value.options = options;
    return value;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// FLOAT text

TEST("core/value text: float with volt gets one decimal") {
    CHECK_EQ(text(Value(1.0f, UNIT_VOLT)), "1.0 V");
    CHECK_EQ(text(Value(12.0f, UNIT_AMPER)), "12.0 A");
    CHECK_EQ(text(Value(-2.5f, UNIT_VOLT)), "-2.5 V");
}

TEST("core/value text: float zero and negative zero") {
    CHECK_EQ(text(Value(0.0f, UNIT_VOLT)), "0.0 V");
    CHECK_EQ(text(Value(-0.0f, UNIT_VOLT)), "0.0 V");
    CHECK_EQ(text(Value(0.0f, UNIT_SECOND)), "0 s");
}

TEST("core/value text: float without a unit drops trailing zeros") {
    CHECK_EQ(text(Value(2.5f, VALUE_TYPE_FLOAT)), "2.5");
    CHECK_EQ(text(Value(3.0f, UNIT_NONE)), "3");
    CHECK_EQ(text(Value(0.125f, UNIT_PERCENT)), "0.125 %");
}

TEST("core/value text: float of a base unit switches to a smaller unit") {
    CHECK_EQ(text(Value(0.0015f, UNIT_AMPER)), "1.5 mA");
    CHECK_EQ(text(Value(0.5f, UNIT_HERTZ)), "500 mHz");
    // the ".0" comes from the unit of the value (A), not from uA
    CHECK_EQ(text(Value(0.000002f, UNIT_AMPER)), "2.0 uA");
    CHECK_EQ(text(Value(0.25f, UNIT_SECOND)), "250 ms");
}

TEST("core/value text: float of a base unit switches to a bigger unit") {
    // the number part only, the unit names are covered by CK3
    CHECK(text(Value(4700.0f, UNIT_OHM)).rfind("4.7 ", 0) == 0);
    CHECK(text(Value(2000000.0f, UNIT_OHM)).rfind("2 M", 0) == 0);
    CHECK(text(Value(1500.0f, UNIT_HERTZ)).rfind("1.5 ", 0) == 0);
    // there is no kV: volts stay volts
    CHECK_EQ(text(Value(1500.0f, UNIT_VOLT)), "1500.0 V");
}

TEST("core/value text: watts use two decimals, trailing zeros dropped") {
    CHECK_EQ(text(Value(1.5f, UNIT_WATT)), "1.5 W");
    CHECK_EQ(text(Value(1.234f, UNIT_WATT)), "1.23 W");
    CHECK_EQ(text(Value(2.0f, UNIT_WATT)), "2.0 W");
}

TEST("core/value text: float less-than option") {
    CHECK_EQ(text(Value(2.5f, UNIT_VOLT, FLOAT_OPTIONS_LESS_THEN)), "< 2.5 V");
    // no ".0" after "<"
    CHECK_EQ(text(Value(2.0f, UNIT_VOLT, FLOAT_OPTIONS_LESS_THEN)), "< 2 V");
}

TEST("core/value text: float fixed decimals keep the unit and the zeros") {
    CHECK_EQ(text(Value(2.5f, UNIT_VOLT, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(3))), "2.500 V");
    CHECK_EQ(text(Value(0.0015f, UNIT_AMPER, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4))), "0.0015 A");
    CHECK_EQ(text(Value(7.0f, UNIT_NONE, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(0))), "7");
}

TEST("core/value text: float NaN is empty, infinity is the infinity glyph") {
    CHECK_EQ(text(Value(NAN, UNIT_VOLT)), "");
    CHECK_EQ(text(Value(INFINITY, UNIT_VOLT)), INFINITY_SYMBOL);
    CHECK_EQ(text(Value(-INFINITY, UNIT_VOLT)), INFINITY_SYMBOL);
}

TEST("core/value text: float with 6 significant digits") {
    // pinned: %g, so big values get an exponent
    CHECK_EQ(text(Value(1234567.0f, UNIT_NONE)), "1.23457e+06");
    CHECK_EQ(text(Value(123456.0f, UNIT_NONE)), "123456");
}

TEST("core/value text: float text is cut to the buffer") {
    char buffer[5];
    Value(1.5f, UNIT_VOLT).toText(buffer, sizeof(buffer));
    CHECK_STR(buffer, "1.5 ");
}

TEST("core/value text: float in mV keeps its value") {
    // the value keeps its unit (C1: 0.5 mV was shown as "500 mV")
    CHECK_EQ(text(Value(0.5f, UNIT_MILLI_VOLT)), "0.5 mV");
}

TEST("core/value text: float in kOhm keeps its value") {
    // the value keeps its unit (C1: 4.7 kOhm was divided by 1000 again)
    CHECK(text(Value(4.7f, UNIT_KOHM)).rfind("4.7 ", 0) == 0);
}

////////////////////////////////////////////////////////////////////////////////
// DOUBLE text

TEST("core/value text: double with units like float") {
    CHECK_EQ(text(doubleWithUnit(1.0, UNIT_VOLT)), "1.0 V");
    CHECK_EQ(text(doubleWithUnit(0.0015, UNIT_AMPER)), "1.5 mA");
    CHECK_EQ(text(doubleWithUnit(2.5, UNIT_WATT)), "2.5 W");
    CHECK_EQ(text(Value(0.1, VALUE_TYPE_DOUBLE)), "0.1");
    CHECK_EQ(text(Value((double)NAN, VALUE_TYPE_DOUBLE)), "");
    CHECK_EQ(text(doubleWithUnit(2.5, UNIT_VOLT, FLOAT_OPTIONS_LESS_THEN)), "< 2.5 V");
}

TEST("core/value text: double fixed decimals") {
    CHECK_EQ(text(doubleWithUnit(2.5, UNIT_VOLT, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(2))), "2.50 V");
}

TEST_BUG("core/value text: double fixed decimals keep double precision", "C11") {
    // stringAppendFloat converts to float: 123456789.12 -> 123456792.00
    CHECK_EQ(text(doubleWithUnit(123456789.12, UNIT_UNKNOWN, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(2))), "123456789.12");
}

TEST("core/value text: double in mV keeps its value") {
    CHECK_EQ(text(doubleWithUnit(0.5, UNIT_MILLI_VOLT)), "0.5 mV");
}

////////////////////////////////////////////////////////////////////////////////
// other types

TEST("core/value text: integers") {
    CHECK_EQ(text(Value(-42)), "-42");
    CHECK_EQ(text(Value((uint32_t)4000000000u, VALUE_TYPE_UINT32)), "4000000000");
    CHECK_EQ(text(Value((int8_t)-128, VALUE_TYPE_INT8)), "-128");
    CHECK_EQ(text(Value((uint8_t)255, VALUE_TYPE_UINT8)), "255");
    CHECK_EQ(text(Value((int16_t)-32768, VALUE_TYPE_INT16)), "-32768");
    CHECK_EQ(text(Value((uint16_t)65535, VALUE_TYPE_UINT16)), "65535");
    CHECK_EQ(text(Value((int64_t)-9000000000LL, VALUE_TYPE_INT64)), "-9000000000");
    CHECK_EQ(text(Value((uint64_t)18446744073709551615ULL, VALUE_TYPE_UINT64)), "18446744073709551615");
}

TEST("core/value text: boolean, undefined, null, error, arrays") {
    CHECK_EQ(text(BooleanValue(true)), "true");
    CHECK_EQ(text(BooleanValue(false)), "false");
    CHECK_EQ(text(Value()), "");
    CHECK_EQ(text(Value(0, VALUE_TYPE_NULL)), "");
    CHECK_EQ(text(Value::makeError()), "");
    CHECK_EQ(text(Value::makeArrayRef(2, flow::defs_v3::ARRAY_TYPE_INTEGER, 0)), "");
}

TEST("core/value text: strings are copied and cut to the buffer") {
    CHECK_EQ(text(Value("hello")), "hello");
    CHECK_EQ(text(Value::makeStringRef("ref", -1, 0)), "ref");
    CHECK_EQ(text(Value((const char *)nullptr)), "");

    char buffer[4];
    Value("hello").toText(buffer, sizeof(buffer));
    CHECK_STR(buffer, "hel");
}

TEST("core/value text: IP address and time zone") {
    CHECK_EQ(text(Value(getIpAddress(192, 168, 1, 2), VALUE_TYPE_IP_ADDRESS)), "192.168.1.2");
    CHECK_EQ(text(Value((int16_t)-530, VALUE_TYPE_TIME_ZONE)), "-05:30 GMT");
    CHECK_EQ(text(Value((int16_t)0, VALUE_TYPE_TIME_ZONE)), "GMT");
}

TEST("core/value text: date in the locale format") {
    // FORMAT_DMY_24 by default
    CHECK_EQ(text(Value(1709210096789.0, VALUE_TYPE_DATE)), "29-02-2024 12:34:56.789");
}

TEST("core/value text: stream and widget") {
    CHECK_EQ(text(Value(3, VALUE_TYPE_STREAM)), "stream (id=3)");
    CHECK_EQ(text(Value(5, VALUE_TYPE_WIDGET)), "widget (id=5)");
}

TEST("core/value text: value pointer shows the target") {
    Value target(1.5f, UNIT_VOLT);
    Value p(&target);
    CHECK_EQ(p.getType(), VALUE_TYPE_VALUE_PTR);
    CHECK_EQ(text(p), "1.5 V");
    CHECK_EQ(text(Value((Value *)nullptr)), "");
}

TEST_BUG("core/value text: blob shows its size", "C11") {
    // prints the low 32 bits of the BlobRef pointer
    uint8_t data[5] = { 1, 2, 3, 4, 5 };
    CHECK_EQ(text(Value::makeBlobRef(data, 5, 0)), "blob (size=5)");
}

TEST_BUG("core/value text: versioned string shows the string", "NEW-core-1") {
    // getString() handles only STRING and STRING_REF, so the text is empty
    Value v((uint8_t)3, "versioned");
    CHECK_EQ(v.getType(), VALUE_TYPE_VERSIONED_STRING);
    CHECK_EQ(text(v), "versioned");
}

////////////////////////////////////////////////////////////////////////////////
// toString

TEST("core/value toString: integers of every width") {
    CHECK_EQ(toStr(Value((int8_t)-5, VALUE_TYPE_INT8)), "-5");
    CHECK_EQ(toStr(Value((uint8_t)250, VALUE_TYPE_UINT8)), "250");
    CHECK_EQ(toStr(Value((int16_t)-300, VALUE_TYPE_INT16)), "-300");
    CHECK_EQ(toStr(Value((uint16_t)60000, VALUE_TYPE_UINT16)), "60000");
    CHECK_EQ(toStr(Value(-70000)), "-70000");
    CHECK_EQ(toStr(Value((uint32_t)4000000000u, VALUE_TYPE_UINT32)), "4000000000");
    CHECK_EQ(toStr(Value((int64_t)-9000000000LL, VALUE_TYPE_INT64)), "-9000000000");
    CHECK_EQ(toStr(Value((uint64_t)10000000000ULL, VALUE_TYPE_UINT64)), "10000000000");
}

TEST("core/value toString: floating point with %g") {
    CHECK_EQ(toStr(Value(2.5, VALUE_TYPE_DOUBLE)), "2.5");
    CHECK_EQ(toStr(Value(0.1, VALUE_TYPE_DOUBLE)), "0.1");
    // the unit is not part of toString
    CHECK_EQ(toStr(Value(2.5f, UNIT_VOLT)), "2.5");
}

TEST("core/value toString: other types use toText") {
    CHECK_EQ(toStr(BooleanValue(true)), "true");
    CHECK_EQ(toStr(Value()), "");
    CHECK_EQ(toStr(Value(1709210096789.0, VALUE_TYPE_DATE)), "29-02-2024 12:34:56.789");
}

TEST("core/value toString: a string is returned as it is") {
    const char *str = "same";
    Value v(str);
    Value s = v.toString(0);
    CHECK_EQ(s.getType(), VALUE_TYPE_STRING);
    CHECK(s.getString() == str);

    Value ref = Value::makeStringRef("ref", -1, 0);
    Value s2 = ref.toString(0);
    CHECK(s2.refValue == ref.refValue);
    CHECK_EQ((int)ref.refValue->refCounter, 2);
}

TEST("core/value toString: makes a new string ref") {
    Value s = Value(42).toString(0);
    CHECK_EQ(s.getType(), VALUE_TYPE_STRING_REF);
    CHECK_EQ((int)s.refValue->refCounter, 1);
    CHECK_STR(s.getString(), "42");
}

TEST_BUG("core/value toString: double keeps all significant digits", "C11") {
    // %g: a date in ms becomes 1.73e+12 when it is concatenated into a string
    CHECK_EQ(toStr(Value(1730000000123.0, VALUE_TYPE_DOUBLE)), "1730000000123");
}

////////////////////////////////////////////////////////////////////////////////
// type names

TEST("core/value: type names") {
    CHECK_STR(typeName(Value()), "undefined");
    CHECK_STR(typeName(Value(0, VALUE_TYPE_NULL)), "null");
    CHECK_STR(typeName(BooleanValue(true)), "boolean");
    CHECK_STR(typeName(Value((int8_t)1, VALUE_TYPE_INT8)), "int8");
    CHECK_STR(typeName(Value((uint8_t)1, VALUE_TYPE_UINT8)), "uint8");
    CHECK_STR(typeName(Value((int16_t)1, VALUE_TYPE_INT16)), "int16");
    CHECK_STR(typeName(Value((uint16_t)1, VALUE_TYPE_UINT16)), "uint16");
    CHECK_STR(typeName(Value(1)), "int32");
    CHECK_STR(typeName(Value((uint32_t)1, VALUE_TYPE_UINT32)), "uint32");
    CHECK_STR(typeName(Value((int64_t)1, VALUE_TYPE_INT64)), "int64");
    CHECK_STR(typeName(Value((uint64_t)1, VALUE_TYPE_UINT64)), "uint64");
    CHECK_STR(typeName(Value(1.0f, UNIT_NONE)), "float");
    CHECK_STR(typeName(Value(1.0, VALUE_TYPE_DOUBLE)), "double");
    CHECK_STR(typeName(Value("a")), "string");
    CHECK_STR(typeName(Value::makeStringRef("a", -1, 0)), "string");
    CHECK_STR(typeName(Value::makeArrayRef(1, flow::defs_v3::ARRAY_TYPE_INTEGER, 0)), "array");
    CHECK_STR(typeName(Value::makeBlobRef(nullptr, 1, 0)), "blob");
    CHECK_STR(typeName(Value(1.0, VALUE_TYPE_DATE)), "date");
    CHECK_STR(typeName(Value::makeError()), "error");
    CHECK_STR(typeName(Value(1, VALUE_TYPE_STREAM)), "stream");
    CHECK_STR(typeName(Value(1, VALUE_TYPE_WIDGET)), "widget");
    CHECK_STR(typeName(Value(1, VALUE_TYPE_JSON)), "json");
}

TEST("core/value: type name of a value pointer is the target's") {
    Value target(1.5f, UNIT_VOLT);
    CHECK_STR(typeName(Value(&target)), "float");
    CHECK_STR(typeName(Value((Value *)nullptr)), "null");
}

TEST_BUG("core/value: type name of a pointer to a value pointer", "C11") {
    // the implicit Value(Value *) wraps the pointer again: endless recursion
    Value target(5);
    Value inner(&target);
    Value outer(&inner);
    CHECK_STR(typeName(outer), "int32");
}
