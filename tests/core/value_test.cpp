/*
 * eez-framework tests: core/value (constructors, getters, conversions, compare)
 */

#include <math.h>
#include <string.h>
#include <new>

#include <eez/conf-internal.h>
#include <eez/core/value.h>
#include <eez/core/util.h>

#include "test.h"

using namespace eez;

// Value constructed into memory filled with a pattern, so bytes the constructor
// doesn't set are visible
template <typename T>
static Value *constructOverPattern(void *buffer, T value, ValueType type) {
    memset(buffer, 0xCC, sizeof(Value));
    return new (buffer) Value(value, type);
}

TEST("core/value: int32 round trip") {
    Value v(-123, VALUE_TYPE_INT32);
    CHECK_EQ(v.getType(), VALUE_TYPE_INT32);
    CHECK_EQ(v.getInt(), -123);
    CHECK_EQ(v.toDouble(), -123.0);
}

TEST("core/value: string ref copy and compare") {
    Value a = Value::makeStringRef("hello", 5, 0x1234);
    Value b = a;
    CHECK_STR(b.getString(), "hello");
    CHECK(a == b);
}

TEST("core/value: float text with base unit") {
    char text[64];
    Value(2.5f, UNIT_VOLT).toText(text, sizeof(text));
    CHECK_STR(text, "2.5 V");
}

TEST("core/value: float text with a non-base unit keeps the value") {
    // 720 minutes (BB3+ NTP refresh default) keeps its unit, a derived unit is
    // chosen only for a value in a base unit (C1: it was "12 minutes")
    char text[64];
    Value(720.0f, UNIT_MINUTE).toText(text, sizeof(text));
    CHECK(strstr(text, "720") != nullptr);
}

TEST("core/value: uint8 value reads back as int") {
    alignas(Value) unsigned char buffer[sizeof(Value)];
    auto v = constructOverPattern(buffer, (uint8_t)5, VALUE_TYPE_UINT8);
    CHECK_EQ(v->getInt(), 5);
}

TEST("core/value: int8 value is sign extended") {
    alignas(Value) unsigned char buffer[sizeof(Value)];
    auto v = constructOverPattern(buffer, (int8_t)-1, VALUE_TYPE_INT8);
    CHECK_EQ(v->getInt(), -1);
}

TEST_BUG("core/util: stringCopyLength stays inside the buffer", "C8") {
    char buffer[8];
    memset(buffer, 'x', sizeof(buffer));
    stringCopyLength(buffer, 4, "abcdef", 6);
    CHECK_EQ(buffer[4], 'x');
    CHECK_EQ((int)strlen(buffer), 3);
}

////////////////////////////////////////////////////////////////////////////////
// constructors and getters

TEST("core/value: default value is undefined") {
    Value v;
    CHECK_EQ(v.getType(), VALUE_TYPE_UNDEFINED);
    CHECK_EQ(v.getUnit(), UNIT_UNKNOWN);
    CHECK_EQ((int)v.getOptions(), 0);
    CHECK_EQ(v.getUInt64(), (uint64_t)0);
    CHECK(v.isUndefinedOrNull());
}

TEST("core/value: int and const char * constructors pick INT32 and STRING") {
    Value i(7);
    CHECK_EQ(i.getType(), VALUE_TYPE_INT32);
    CHECK_EQ(i.getInt32(), 7);

    const char *str = "abc";
    Value s(str);
    CHECK_EQ(s.getType(), VALUE_TYPE_STRING);
    CHECK(s.getString() == str);
    CHECK(s.isString());
}

TEST("core/value: unsigned and 64-bit integers keep their range") {
    Value u32((uint32_t)4000000000u, VALUE_TYPE_UINT32);
    CHECK_EQ(u32.getUInt32(), 4000000000u);
    CHECK_EQ(u32.toDouble(), 4000000000.0);
    CHECK_EQ(u32.toInt64(), (int64_t)4000000000LL);

    Value i64((int64_t)-9000000000LL, VALUE_TYPE_INT64);
    CHECK_EQ(i64.getInt64(), (int64_t)-9000000000LL);
    CHECK_EQ(i64.toInt64(), (int64_t)-9000000000LL);
    CHECK_EQ(i64.toDouble(), -9000000000.0);

    Value u64((uint64_t)18446744073709551615ULL, VALUE_TYPE_UINT64);
    CHECK_EQ(u64.getUInt64(), (uint64_t)18446744073709551615ULL);
}

TEST("core/value: narrow integer getters and conversions use the narrow member") {
    Value i8((int8_t)-5, VALUE_TYPE_INT8);
    CHECK_EQ((int)i8.getInt8(), -5);
    CHECK_EQ(i8.toInt32(), -5);
    CHECK_EQ(i8.toInt64(), (int64_t)-5);
    CHECK_EQ(i8.toDouble(), -5.0);
    CHECK(i8.toBool());

    Value u8((uint8_t)200, VALUE_TYPE_UINT8);
    CHECK_EQ((int)u8.getUInt8(), 200);
    CHECK_EQ(u8.toInt32(), 200);

    Value i16((int16_t)-30000, VALUE_TYPE_INT16);
    CHECK_EQ((int)i16.getInt16(), -30000);
    CHECK_EQ(i16.toInt32(), -30000);
    CHECK_EQ(i16.toFloat(), -30000.0f);

    Value u16((uint16_t)65535, VALUE_TYPE_UINT16);
    CHECK_EQ((int)u16.getUInt16(), 65535);
    CHECK_EQ(u16.toInt32(), 65535);
    CHECK_EQ(u16.toDouble(), 65535.0);
}

TEST("core/value: uint16 value reads back as int") {
    alignas(Value) unsigned char buffer[sizeof(Value)];
    auto v = constructOverPattern(buffer, (uint16_t)1000, VALUE_TYPE_UINT16);
    CHECK_EQ(v->getInt(), 1000);
}

TEST("core/value: int16 value is sign extended") {
    alignas(Value) unsigned char buffer[sizeof(Value)];
    auto v = constructOverPattern(buffer, (int16_t)-2, VALUE_TYPE_INT16);
    CHECK_EQ(v->getInt(), -2);
}

TEST("core/value: float and double constructors") {
    Value f(1.25f, UNIT_AMPER);
    CHECK_EQ(f.getType(), VALUE_TYPE_FLOAT);
    CHECK_EQ(f.getUnit(), UNIT_AMPER);
    CHECK_EQ(f.getFloat(), 1.25f);
    CHECK_EQ(f.toDouble(), 1.25);

    Value f2(3.5f, UNIT_VOLT, FLOAT_OPTIONS_LESS_THEN);
    CHECK_EQ((int)f2.getOptions(), FLOAT_OPTIONS_LESS_THEN);

    Value f3(0.5f, VALUE_TYPE_FLOAT);
    CHECK_EQ(f3.getUnit(), UNIT_UNKNOWN);

    Value d(0.1, VALUE_TYPE_DOUBLE);
    CHECK_EQ(d.getType(), VALUE_TYPE_DOUBLE);
    CHECK_EQ(d.getDouble(), 0.1);
    CHECK_EQ(d.toFloat(), 0.1f);
    CHECK(d.isDouble());
}

TEST("core/value: helper constructors") {
    CHECK_EQ(IntegerValue(-3).getType(), VALUE_TYPE_INT32);
    CHECK_EQ(IntegerValue(-3).getInt(), -3);
    CHECK_EQ(FloatValue(1.5f).getType(), VALUE_TYPE_FLOAT);
    CHECK_EQ(DoubleValue(2.5).getType(), VALUE_TYPE_DOUBLE);

    Value b = BooleanValue(true);
    CHECK_EQ(b.getType(), VALUE_TYPE_BOOLEAN);
    CHECK(b.getBoolean());
    CHECK_EQ(b.getInt(), 1);

    Value s = StringValue("abc");
    CHECK_EQ(s.getType(), VALUE_TYPE_STRING_REF);
    CHECK_STR(s.getString(), "abc");
}

TEST("core/value: type predicates") {
    CHECK(Value((int8_t)1, VALUE_TYPE_INT8).isInt32OrLess());
    CHECK(Value((uint32_t)1, VALUE_TYPE_UINT32).isInt32OrLess());
    CHECK(BooleanValue(true).isInt32OrLess());
    CHECK(!Value((int64_t)1, VALUE_TYPE_INT64).isInt32OrLess());
    CHECK(Value((uint64_t)1, VALUE_TYPE_UINT64).isInt64());
    CHECK(Value(1, VALUE_TYPE_UINT32).isInt32());
    CHECK(Value((int16_t)1, VALUE_TYPE_INT16).isInt16());
    CHECK(Value((uint8_t)1, VALUE_TYPE_UINT8).isInt8());
    CHECK(Value(1.0, VALUE_TYPE_DATE).isDouble());
    CHECK(!Value(1.0f, VALUE_TYPE_FLOAT).isDouble());
    CHECK(Value("a", VALUE_TYPE_STRING_ASSET).isString());
    CHECK(Value(0, VALUE_TYPE_NULL).isUndefinedOrNull());
    CHECK(Value::makeError().isError());
    CHECK(Value(0, VALUE_TYPE_JSON).isJson());
    CHECK(Value(0, VALUE_TYPE_WIDGET).isWidget());
}

TEST("core/value: range and enum values") {
    Value range = MakeRangeValue(3, 7);
    CHECK_EQ(range.getType(), VALUE_TYPE_RANGE);
    CHECK_EQ((int)range.getRangeFrom(), 3);
    CHECK_EQ((int)range.getRangeTo(), 7);

    Value e = MakeEnumDefinitionValue(2, 1);
    CHECK_EQ(e.getType(), VALUE_TYPE_ENUM);
    CHECK_EQ(e.getInt(), 2);
    CHECK_EQ((int)e.getEnum().enumDefinition, 1);
}

TEST("core/value: page index and count from a pair of uint16") {
    Value v;
    v.pairOfUint16Value.first = 4;
    v.pairOfUint16Value.second = 9;
    CHECK_EQ((int)getPageIndexFromValue(v), 4);
    CHECK_EQ((int)getNumPagesFromValue(v), 9);
}

TEST("core/value: pointer values") {
    int target = 0;
    Value p(&target, VALUE_TYPE_POINTER);
    CHECK(p.getVoidPointer() == &target);
    CHECK(p == Value(&target, VALUE_TYPE_POINTER));
    int other = 0;
    CHECK(p != Value(&other, VALUE_TYPE_POINTER));
}

TEST_BUG("core/value: a float stored with the DOUBLE type reads back as the same number", "CK1") {
    // the payload type picks the union member, the tag is separate: float bits
    // are stored under the DOUBLE tag (API hazard)
    Value v(2.5f, VALUE_TYPE_DOUBLE);
    CHECK_EQ(v.toDouble(), 2.5);
}

////////////////////////////////////////////////////////////////////////////////
// conversions

TEST("core/value: toBool") {
    CHECK(!Value().toBool());
    CHECK(!Value(0, VALUE_TYPE_NULL).toBool());
    CHECK(!Value(0).toBool());
    CHECK(Value(-1).toBool());
    CHECK(!Value(0.0, VALUE_TYPE_DOUBLE).toBool());
    CHECK(Value(0.5f, UNIT_NONE).toBool());
    CHECK(!Value("").toBool());
    CHECK(Value("0").toBool());
    CHECK(Value((uint64_t)1 << 40, VALUE_TYPE_UINT64).toBool());
    CHECK(Value(1.0, VALUE_TYPE_DATE).toBool());
}

TEST("core/value: toBool of arrays and blobs is their size") {
    CHECK(!Value::makeArrayRef(0, flow::defs_v3::ARRAY_TYPE_INTEGER, 0).toBool());
    CHECK(Value::makeArrayRef(2, flow::defs_v3::ARRAY_TYPE_INTEGER, 0).toBool());
    CHECK(!Value::makeBlobRef(nullptr, 0, 0).toBool());
    uint8_t data[2] = { 1, 2 };
    CHECK(Value::makeBlobRef(data, 2, 0).toBool());
}

TEST("core/value: toBool of a value without a boolean meaning reports an error") {
    int err = 0;
    CHECK(!Value(0, VALUE_TYPE_RANGE).toBool(&err));
    CHECK_EQ(err, 1);
    err = 1;
    CHECK(Value(1).toBool(&err));
    CHECK_EQ(err, 0);
}

TEST("core/value: toDouble of strings") {
    int err = -1;
    CHECK_EQ(Value("2.5").toDouble(&err), 2.5);
    CHECK_EQ(err, 0);
    // whitespace around the number is accepted
    CHECK_EQ(Value(" -1e3  ").toDouble(&err), -1000.0);
    CHECK_EQ(err, 0);

    double d = Value("2.5x").toDouble(&err);
    CHECK(isNaN(d));
    CHECK_EQ(err, 1);

    // pinned: strtod reads nothing from "", but the end of the string is
    // reached, so "" converts to 0 without an error
    d = Value("").toDouble(&err);
    CHECK_EQ(d, 0.0);
    CHECK_EQ(err, 0);
}

TEST("core/value: toDouble of a value without a number is NaN with an error") {
    int err = 0;
    CHECK(isNaN(Value().toDouble(&err)));
    CHECK_EQ(err, 1);
    err = 0;
    CHECK(isNaN(Value(0, VALUE_TYPE_NULL).toFloat(&err)));
    CHECK_EQ(err, 1);
}

TEST("core/value: toFloat of strings and numbers") {
    int err = -1;
    CHECK_EQ(Value("0.25").toFloat(&err), 0.25f);
    CHECK_EQ(err, 0);
    CHECK_EQ(Value(0.5, VALUE_TYPE_DOUBLE).toFloat(), 0.5f);
    CHECK_EQ(Value((int64_t)1 << 24, VALUE_TYPE_INT64).toFloat(), 16777216.0f);
    CHECK(isNaN(Value("x").toFloat(&err)));
    CHECK_EQ(err, 1);
}

TEST("core/value: toInt32 truncates floating point values toward zero") {
    CHECK_EQ(Value(3.9, VALUE_TYPE_DOUBLE).toInt32(), 3);
    CHECK_EQ(Value(-3.9, VALUE_TYPE_DOUBLE).toInt32(), -3);
    CHECK_EQ(Value(2.99f, UNIT_NONE).toInt32(), 2);
}

TEST("core/value: toInt32 of a uint32 above INT32_MAX wraps") {
    // pinned: the bits are reinterpreted
    CHECK_EQ(Value((uint32_t)4000000000u, VALUE_TYPE_UINT32).toInt32(), (int32_t)-294967296);
    CHECK_EQ(Value((int64_t)0x100000005LL, VALUE_TYPE_INT64).toInt32(), 5);
}

TEST("core/value: toInt32 of strings") {
    int err = -1;
    CHECK_EQ(Value("42").toInt32(&err), 42);
    CHECK_EQ(err, 0);
    CHECK_EQ(Value(" -7 ").toInt32(&err), -7);
    CHECK_EQ(err, 0);
    // not an integer: 0 with an error
    CHECK_EQ(Value("4.5").toInt32(&err), 0);
    CHECK_EQ(err, 1);
    CHECK_EQ(Value("abc").toInt32(&err), 0);
    CHECK_EQ(err, 1);
}

TEST("core/value: toInt64 of a string above the int32 range") {
    int err = -1;
    CHECK_EQ(Value("5000000000").toInt64(&err), (int64_t)5000000000LL);
    CHECK_EQ(err, 0);
}

TEST_BUG("core/value: toInt32 of a string above the int32 range is an error", "C10") {
    // strtol reads 5000000000 (64-bit long on CM5), the int it is stored in wraps
    int err = 0;
    int32_t v = Value("5000000000").toInt32(&err);
    CHECK(err != 0 || v == INT32_MAX);
}

TEST("core/value: toInt32 of a value without a number is 0 with an error") {
    int err = 0;
    CHECK_EQ(Value().toInt32(&err), 0);
    CHECK_EQ(err, 1);
    err = 0;
    CHECK_EQ(Value().toInt64(&err), (int64_t)0);
    CHECK_EQ(err, 1);
}

TEST("core/value: boolean converts like 0 and 1") {
    Value t = BooleanValue(true);
    CHECK_EQ(t.toInt32(), 1);
    CHECK_EQ(t.toInt64(), (int64_t)1);
    CHECK_EQ(t.toDouble(), 1.0);
    CHECK_EQ(BooleanValue(false).toFloat(), 0.0f);
}

TEST("core/value: date converts to its milliseconds") {
    Value d(1709210096789.0, VALUE_TYPE_DATE);
    CHECK_EQ(d.toDouble(), 1709210096789.0);
    CHECK(d.toBool());
}

TEST_BUG("core/value: toInt64 of a date is its milliseconds", "NEW-core-3") {
    // toDouble and toBool handle DATE, toInt64 (and toInt32, toFloat) don't:
    // 0 with an error
    int err = 0;
    CHECK_EQ(Value(1709210096789.0, VALUE_TYPE_DATE).toInt64(&err), (int64_t)1709210096789LL);
    CHECK_EQ(err, 0);
}

TEST("core/value: assignValue converts to the destination type") {
    Value dst;
    assignValue(dst, Value(5), VALUE_TYPE_BOOLEAN);
    CHECK_EQ(dst.getType(), VALUE_TYPE_BOOLEAN);
    CHECK(dst.getBoolean());

    assignValue(dst, Value("42"), VALUE_TYPE_INT32);
    CHECK_EQ(dst.getType(), VALUE_TYPE_INT32);
    CHECK_EQ(dst.getInt(), 42);

    assignValue(dst, Value(2.5, VALUE_TYPE_DOUBLE), VALUE_TYPE_FLOAT);
    CHECK_EQ(dst.getType(), VALUE_TYPE_FLOAT);
    CHECK_EQ(dst.getFloat(), 2.5f);

    assignValue(dst, Value(7), VALUE_TYPE_DOUBLE);
    CHECK_EQ(dst.getType(), VALUE_TYPE_DOUBLE);
    CHECK_EQ(dst.getDouble(), 7.0);

    assignValue(dst, Value(42), VALUE_TYPE_STRING);
    CHECK(dst.isString());
    CHECK_STR(dst.getString(), "42");
}

TEST("core/value: assignValue to a narrow type keeps the int32 without clamping") {
    // pinned: 300 into UINT8 stores int32 300, the uint8 member reads 44
    Value dst;
    assignValue(dst, Value(300), VALUE_TYPE_UINT8);
    CHECK_EQ(dst.getType(), VALUE_TYPE_UINT8);
    CHECK_EQ(dst.getInt(), 300);
    CHECK_EQ((int)dst.getUInt8(), 44);
}

TEST("core/value: assignValue without a type keeps the destination's type") {
    Value dst(1.0f, UNIT_VOLT);
    assignValue(dst, Value("2.5"));
    CHECK_EQ(dst.getType(), VALUE_TYPE_FLOAT);
    CHECK_EQ(dst.getUnit(), UNIT_VOLT);
    CHECK_EQ(dst.getFloat(), 2.5f);

    Value dstInt(1);
    assignValue(dstInt, Value(9.7, VALUE_TYPE_DOUBLE));
    CHECK_EQ(dstInt.getType(), VALUE_TYPE_INT32);
    CHECK_EQ(dstInt.getInt(), 9);

    Value dstAny;
    assignValue(dstAny, Value(9.7, VALUE_TYPE_DOUBLE));
    CHECK_EQ(dstAny.getType(), VALUE_TYPE_DOUBLE);
}

////////////////////////////////////////////////////////////////////////////////
// compare

TEST("core/value: integers compare by type and value") {
    CHECK(Value(5) == Value(5));
    CHECK(Value(5) != Value(6));
    CHECK(Value(5) != Value(5, VALUE_TYPE_UINT32));
    CHECK(Value((int8_t)-1, VALUE_TYPE_INT8) == Value((int8_t)-1, VALUE_TYPE_INT8));
    CHECK(Value((int64_t)1 << 40, VALUE_TYPE_INT64) != Value((int64_t)1 << 41, VALUE_TYPE_INT64));
}

TEST("core/value: floats compare by value, unit and options") {
    CHECK(Value(1.5f, UNIT_VOLT) == Value(1.5f, UNIT_VOLT));
    CHECK(Value(1.5f, UNIT_VOLT) != Value(1.5f, UNIT_AMPER));
    CHECK(Value(1.5f, UNIT_VOLT) != Value(1.5f, UNIT_VOLT, FLOAT_OPTIONS_LESS_THEN));
    CHECK(Value(1.5, VALUE_TYPE_DOUBLE) == Value(1.5, VALUE_TYPE_DOUBLE));
    CHECK(Value(1.5, VALUE_TYPE_DOUBLE) != Value(1.5f, VALUE_TYPE_FLOAT));
}

TEST("core/value: strings compare by content across string types") {
    Value a("abc");
    Value b = Value::makeStringRef("abc", -1, 0);
    CHECK(a == b);
    CHECK(b == a);
    CHECK(a != Value("abd"));
    CHECK(a != Value(1));
    CHECK(Value((const char *)nullptr) == Value((const char *)nullptr));
    CHECK(Value((const char *)nullptr) != Value(""));
}

TEST("core/value: undefined, null and error compare") {
    CHECK(Value() == Value());
    CHECK(Value() != Value(0, VALUE_TYPE_NULL));
    CHECK(Value(0, VALUE_TYPE_NULL) == Value(0, VALUE_TYPE_NULL));
    // pinned: an error value is never equal to another value, even itself
    Value e = Value::makeError();
    CHECK(!(e == e));
}

TEST("core/value: dates compare by milliseconds") {
    CHECK(Value(1000.0, VALUE_TYPE_DATE) == Value(1000.0, VALUE_TYPE_DATE));
    CHECK(Value(1000.0, VALUE_TYPE_DATE) != Value(1001.0, VALUE_TYPE_DATE));
    CHECK(Value(1000.0, VALUE_TYPE_DATE) != Value(1000.0, VALUE_TYPE_DOUBLE));
}

TEST("core/value: a float NaN equals the same NaN, no redraw every frame") {
    // display_data compares the old and the new value: a NaN is the same NaN
    // (CP3: a NaN was always "changed")
    Value a(NAN, UNIT_VOLT);
    Value b(NAN, UNIT_VOLT);
    CHECK(a == b);
}

TEST("core/value: a double NaN equals the same NaN") {
    Value a((double)NAN, VALUE_TYPE_DOUBLE);
    CHECK(a == a);
}
