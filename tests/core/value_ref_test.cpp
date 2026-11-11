/*
 * eez-framework tests: core/value references (string, array, blob refs, ref
 * counting, clone, indirect values, typed array helpers)
 */

#include <string.h>

#include <eez/conf-internal.h>
#include <eez/core/value.h>
#include <eez/flow/hooks.h>
#include <eez/flow/flow_defs_v3.h>

#include "test.h"

using namespace eez;
using namespace eez::flow::defs_v3;

namespace {

uint32_t refCount(const Value &value) {
    return value.refValue->refCounter;
}

// a VALUE_TYPE_STRING_ASSET value: the string is at an offset from the value
struct StringAsset {
    Value value;
    char str[16];

    StringAsset(const char *text) {
        strcpy(str, text);
        value.type = VALUE_TYPE_STRING_ASSET;
        value.unit = UNIT_UNKNOWN;
        value.int32Value = (int32_t)((uint8_t *)str - (uint8_t *)&value.int32Value);
    }
};

// a VALUE_TYPE_ARRAY_ASSET value: the array (two integers) is at an offset
// from the value
struct ArrayAsset {
    Value value;
    uint32_t arraySize;
    uint32_t arrayType;
    Value values[2];

    ArrayAsset() : arraySize(2), arrayType(ARRAY_TYPE_INTEGER) {
        values[0] = Value(11);
        values[1] = Value(22);
        value.type = VALUE_TYPE_ARRAY_ASSET;
        value.int32Value = (int32_t)((uint8_t *)&arraySize - (uint8_t *)&value.int32Value);
    }
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// string refs

TEST("core/value refs: makeStringRef copies the string") {
    char source[] = "hello";
    Value v = Value::makeStringRef(source, -1, 0);
    source[0] = 'j';
    CHECK_EQ(v.getType(), VALUE_TYPE_STRING_REF);
    CHECK_EQ((int)v.getOptions(), VALUE_OPTIONS_REF);
    CHECK_EQ(refCount(v), 1u);
    CHECK_STR(v.getString(), "hello");
}

TEST("core/value refs: makeStringRef with a length takes a prefix") {
    Value v = Value::makeStringRef("hello world", 5, 0);
    CHECK_STR(v.getString(), "hello");
    Value empty = Value::makeStringRef("abc", 0, 0);
    CHECK_STR(empty.getString(), "");
    CHECK(!empty.toBool());
}

TEST("core/value refs: concatenateString") {
    Value a("ab");
    Value b = Value::makeStringRef("cd", -1, 0);
    Value c = Value::concatenateString(a, b);
    CHECK_EQ(c.getType(), VALUE_TYPE_STRING_REF);
    CHECK_STR(c.getString(), "abcd");
    Value d = Value::concatenateString(c, Value(""));
    CHECK_STR(d.getString(), "abcd");
}

TEST("core/value refs: copies share the ref and count it") {
    Value a = Value::makeStringRef("x", -1, 0);
    {
        Value b = a;
        CHECK(b.refValue == a.refValue);
        CHECK_EQ(refCount(a), 2u);
        Value c;
        c = b;
        CHECK_EQ(refCount(a), 3u);
    }
    CHECK_EQ(refCount(a), 1u);
}

TEST("core/value refs: assignment releases the old ref") {
    Value a = Value::makeStringRef("a", -1, 0);
    Value b = Value::makeStringRef("b", -1, 0);
    Value keepB = b;
    CHECK_EQ(refCount(keepB), 2u);
    b = a;
    CHECK_EQ(refCount(keepB), 1u);
    CHECK_EQ(refCount(a), 2u);
    CHECK_STR(b.getString(), "a");
    b = Value(5);
    CHECK_EQ(refCount(a), 1u);
    CHECK_EQ(b.getType(), VALUE_TYPE_INT32);
}

TEST("core/value refs: self assignment keeps the value") {
    // operator= releases the old value last (C5: it freed the only ref, then
    // read it)
    Value a = Value::makeStringRef("self", -1, 0);
    Value &alias = a;
    a = alias;
    CHECK_STR(a.getString(), "self");
}

TEST("core/value refs: assigning an element of the array to the array") {
    // operator= releases the old value last (C5: freeRef freed the array before
    // the element was read)
    Value arr = Value::makeArrayRef(1, ARRAY_TYPE_STRING, 0);
    arr.getArray()->values[0] = Value::makeStringRef("element", -1, 0);
    arr = arr.getArray()->values[0];
    CHECK_STR(arr.getString(), "element");
}

TEST("core/value refs: getString of a value that is not a string is null") {
    CHECK(Value(5).getString() == nullptr);
    CHECK(Value().getString() == nullptr);
    CHECK(Value(1.0, VALUE_TYPE_DOUBLE).getString() == nullptr);
}

TEST("core/value refs: a string asset copies into a STRING") {
    StringAsset asset("asset text");
    CHECK_STR(asset.value.getString(), "asset text");
    Value copy = asset.value;
    CHECK_EQ(copy.getType(), VALUE_TYPE_STRING);
    CHECK(copy.getString() == asset.str);
    CHECK(copy == Value("asset text"));
}

TEST_BUG("core/value refs: a string asset copy has an unknown unit", "CK1") {
    // operator= sets unit = 0 (UNIT_NONE) for STRING_ASSET, not UNIT_UNKNOWN
    StringAsset asset("a");
    Value copy = asset.value;
    CHECK_EQ(copy.getUnit(), UNIT_UNKNOWN);
}

TEST_BUG("core/value refs: assigning a string asset replaces dstValueType", "CK1") {
    // operator= keeps the destination's old dstValueType for STRING_ASSET
    StringAsset asset("a");
    Value dst;
    dst.dstValueType = VALUE_TYPE_INT32;
    dst = asset.value;
    CHECK_EQ((int)dst.dstValueType, (int)asset.value.dstValueType);
}

TEST("core/value refs: an array asset copies into an ARRAY") {
    ArrayAsset asset;
    const ArrayValue *array = asset.value.getArray();
    CHECK((const void *)array == (const void *)&asset.arraySize);
    CHECK_EQ(array->arraySize, 2u);
    CHECK_EQ(array->values[1].getInt(), 22);
    CHECK(asset.value.isArray());
    CHECK(asset.value.toBool());

    Value copy = asset.value;
    CHECK_EQ(copy.getType(), VALUE_TYPE_ARRAY);
    CHECK(copy.getArray() == array);
    CHECK_EQ(Value::makeArrayElementRef(copy, 0, 0).toInt32(), 11);
}

TEST("core/value refs: copy keeps dstValueType for other types") {
    Value src(5);
    src.dstValueType = VALUE_TYPE_DOUBLE;
    Value dst;
    dst = src;
    CHECK_EQ((int)dst.dstValueType, (int)VALUE_TYPE_DOUBLE);
}

////////////////////////////////////////////////////////////////////////////////
// array refs

TEST("core/value refs: makeArrayRef") {
    Value arr = Value::makeArrayRef(3, ARRAY_TYPE_INTEGER, 0);
    CHECK_EQ(arr.getType(), VALUE_TYPE_ARRAY_REF);
    CHECK(arr.isArray());
    CHECK_EQ(refCount(arr), 1u);
    auto array = arr.getArray();
    CHECK_EQ(array->arraySize, 3u);
    CHECK_EQ(array->arrayType, (uint32_t)ARRAY_TYPE_INTEGER);
    for (int i = 0; i < 3; i++) {
        CHECK_EQ(array->values[i].getType(), VALUE_TYPE_UNDEFINED);
    }
}

TEST("core/value refs: an empty array") {
    Value arr = Value::makeArrayRef(0, ARRAY_TYPE_INTEGER, 0);
    CHECK_EQ(arr.getArray()->arraySize, 0u);
    CHECK(!arr.toBool());
}

TEST("core/value refs: array elements hold refs until the array is freed") {
    Value s = Value::makeStringRef("element", -1, 0);
    {
        Value arr = Value::makeArrayRef(2, ARRAY_TYPE_STRING, 0);
        arr.getArray()->values[0] = s;
        arr.getArray()->values[1] = s;
        CHECK_EQ(refCount(s), 3u);
    }
    CHECK_EQ(refCount(s), 1u);
}

TEST("core/value refs: arrays compare by identity") {
    Value a = Value::makeArrayRef(1, ARRAY_TYPE_INTEGER, 0);
    Value b = Value::makeArrayRef(1, ARRAY_TYPE_INTEGER, 0);
    Value a2 = a;
    CHECK(a == a2);
    CHECK(a != b);
}

TEST("core/value refs: clone of an array is a deep copy") {
    Value arr = Value::makeArrayRef(3, ARRAY_TYPE_ANY, 0);
    arr.getArray()->values[0] = Value(7);
    arr.getArray()->values[1] = Value::makeStringRef("str", -1, 0);
    Value inner = Value::makeArrayRef(1, ARRAY_TYPE_INTEGER, 0);
    inner.getArray()->values[0] = Value(9);
    arr.getArray()->values[2] = inner;

    Value copy = arr.clone();
    REQUIRE(copy.isArray());
    CHECK(copy.refValue != arr.refValue);
    auto array = copy.getArray();
    CHECK_EQ(array->arraySize, 3u);
    CHECK_EQ(array->arrayType, (uint32_t)ARRAY_TYPE_ANY);
    CHECK_EQ(array->values[0].getInt(), 7);
    CHECK_STR(array->values[1].getString(), "str");
    CHECK(array->values[1].refValue != arr.getArray()->values[1].refValue);
    REQUIRE(array->values[2].isArray());
    CHECK(array->values[2].refValue != inner.refValue);
    CHECK_EQ(array->values[2].getArray()->values[0].getInt(), 9);

    // changing the copy doesn't change the original
    array->values[0] = Value(8);
    CHECK_EQ(arr.getArray()->values[0].getInt(), 7);
}

TEST("core/value refs: clone of a string is a new string ref") {
    Value s("literal");
    Value c = s.clone();
    CHECK_EQ(c.getType(), VALUE_TYPE_STRING_REF);
    CHECK_STR(c.getString(), "literal");
    CHECK(c.getString() != s.getString());
}

TEST("core/value refs: clone of a number is the same value") {
    Value v(2.5f, UNIT_VOLT);
    Value c = v.clone();
    CHECK(c == v);
}

TEST("core/value refs: array element ref reads the element") {
    Value arr = Value::makeArrayRef(2, ARRAY_TYPE_INTEGER, 0);
    arr.getArray()->values[0] = Value(10);
    arr.getArray()->values[1] = Value(20);
    Value element = Value::makeArrayElementRef(arr, 1, 0);
    CHECK_EQ(element.getType(), VALUE_TYPE_ARRAY_ELEMENT_VALUE);
    CHECK(element.isIndirectValueType());
    CHECK_EQ(element.getValue().getInt(), 20);
    CHECK_EQ(element.toInt32(), 20);
    CHECK_EQ(element.toDouble(), 20.0);
    // the element ref holds the array
    CHECK_EQ(refCount(arr), 2u);
}

TEST("core/value refs: array element ref out of range is undefined") {
    Value arr = Value::makeArrayRef(2, ARRAY_TYPE_INTEGER, 0);
    CHECK_EQ(Value::makeArrayElementRef(arr, 2, 0).getValue().getType(), VALUE_TYPE_UNDEFINED);
    CHECK_EQ(Value::makeArrayElementRef(arr, -1, 0).getValue().getType(), VALUE_TYPE_UNDEFINED);
}

TEST("core/value refs: element ref of a blob reads a byte") {
    uint8_t data[3] = { 1, 200, 3 };
    Value blob = Value::makeBlobRef(data, 3, 0);
    Value element = Value::makeArrayElementRef(blob, 1, 0);
    Value v = element.getValue();
    CHECK_EQ(v.getType(), VALUE_TYPE_UINT32);
    CHECK_EQ(v.getUInt32(), 200u);
    CHECK_EQ(Value::makeArrayElementRef(blob, 3, 0).getValue().getType(), VALUE_TYPE_UNDEFINED);
}

namespace {

Value g_dynamicElements[3];

Value getDynamicElement(uint32_t index) {
    return g_dynamicElements[index];
}

void setDynamicElement(uint32_t index, const Value &value) {
    g_dynamicElements[index] = value;
}

const DynamicArrayValue g_dynamicArray = { 3, ARRAY_TYPE_INTEGER, getDynamicElement, setDynamicElement };

} // namespace

TEST("core/value refs: dynamic array") {
    g_dynamicElements[2] = Value(33);
    Value arr = Value::makeDynamicArray(&g_dynamicArray);
    CHECK(arr.isDynamicArray());
    CHECK(arr.getDynamicArray() == &g_dynamicArray);
    CHECK(arr.toBool());
    CHECK_EQ(Value::makeArrayElementRef(arr, 2, 0).toInt32(), 33);
    CHECK_EQ(Value::makeArrayElementRef(arr, 3, 0).getValue().getType(), VALUE_TYPE_UNDEFINED);
    CHECK(arr == Value::makeDynamicArray(&g_dynamicArray));
}

////////////////////////////////////////////////////////////////////////////////
// blob refs

TEST("core/value refs: makeBlobRef copies the data") {
    uint8_t data[4] = { 1, 2, 3, 4 };
    Value blob = Value::makeBlobRef(data, 4, 0);
    data[0] = 9;
    CHECK(blob.isBlob());
    CHECK_EQ(refCount(blob), 1u);
    CHECK_EQ(blob.getBlob()->len, 4u);
    CHECK(memcmp(blob.getBlob()->blob, "\x01\x02\x03\x04", 4) == 0);
}

TEST("core/value refs: makeBlobRef without data is zero filled") {
    Value blob = Value::makeBlobRef(nullptr, 3, 0);
    CHECK_EQ(blob.getBlob()->len, 3u);
    CHECK(memcmp(blob.getBlob()->blob, "\0\0\0", 3) == 0);
}

TEST("core/value refs: makeBlobRef of size 0 is a valid blob on CM5") {
    // CM5 alloc is malloc, malloc(0) succeeds (the simulator allocator returns
    // nullptr for 0 bytes, see CP1)
    Value blob = Value::makeBlobRef(nullptr, 0, 0);
    CHECK_EQ(blob.getType(), VALUE_TYPE_BLOB_REF);
    CHECK_EQ(blob.getBlob()->len, 0u);
}

TEST("core/value refs: makeBlobRef of two parts") {
    uint8_t a[2] = { 1, 2 };
    uint8_t b[3] = { 3, 4, 5 };
    Value blob = Value::makeBlobRef(a, 2, b, 3, 0);
    CHECK_EQ(blob.getBlob()->len, 5u);
    CHECK(memcmp(blob.getBlob()->blob, "\x01\x02\x03\x04\x05", 5) == 0);
}

TEST("core/value refs: blobs compare by identity") {
    uint8_t data[1] = { 1 };
    Value a = Value::makeBlobRef(data, 1, 0);
    Value b = Value::makeBlobRef(data, 1, 0);
    Value a2 = a;
    CHECK(a == a2);
    CHECK(a != b);
}

////////////////////////////////////////////////////////////////////////////////
// indirect values

TEST("core/value refs: value pointer reads through") {
    Value target(41);
    Value p(&target);
    CHECK(p.isIndirectValueType());
    CHECK_EQ(p.toInt32(), 41);
    target = Value(42);
    CHECK_EQ(p.getValue().getInt(), 42);
    CHECK_EQ(p.toDouble(), 42.0);
    CHECK(p.toBool());
}

TEST("core/value refs: value pointers compare by the target") {
    Value a(1), b(1), c(2);
    CHECK(Value(&a) == Value(&a));
    CHECK(Value(&a) == Value(&b));
    CHECK(Value(&a) != Value(&c));
}

TEST("core/value refs: getValue of a direct value is a copy") {
    Value s = Value::makeStringRef("s", -1, 0);
    Value v = s.getValue();
    CHECK(v.refValue == s.refValue);
    CHECK_EQ(refCount(s), 2u);
}

namespace {

Value moduleFieldHook(uint32_t id) {
    if (id == 1) {
        return Value::makeStringRef("module field", -1, 0);
    }
    return Value((int)id * 2);
}

} // namespace

TEST("core/value refs: module field reads through getModuleFieldHook") {
    flow::getModuleFieldHook = moduleFieldHook;
    Value field((int)21, VALUE_TYPE_MODULE_FIELD);
    CHECK(field.isIndirectValueType());
    CHECK_EQ(field.toInt32(), 42);
    CHECK_EQ(field.getValue().getType(), VALUE_TYPE_INT32);
    Value s = Value(1, VALUE_TYPE_MODULE_FIELD).toString(0);
    CHECK_STR(s.getString(), "module field");
}

TEST("core/value refs: getString of a module field string after getValue") {
    // getString() of a computed value asserts (the string ref the hook makes
    // is owned only by the temporary result): the caller keeps the result of
    // getValue() while it uses the string
    flow::getModuleFieldHook = moduleFieldHook;
    Value field(1, VALUE_TYPE_MODULE_FIELD);
    Value value = field.getValue();
    CHECK_STR(value.getString(), "module field");
}

TEST("core/value refs: getString of a value pointer reads the string in place") {
    Value str = Value::makeStringRef("in place", -1, 0);
    Value ptr(&str);
    CHECK(ptr.getString() == str.getString());
    CHECK_EQ(str.refValue->refCounter, 1u);
}

////////////////////////////////////////////////////////////////////////////////
// typed array helpers

TEST("core/value refs: ArrayOfInteger") {
    ArrayOfInteger arr(3);
    CHECK(arr);
    CHECK_EQ(arr.size(), (size_t)3);
    arr.at(1, 7);
    CHECK_EQ(arr.at(1), 7);
    Value v = arr;
    CHECK_EQ(v.getArray()->arrayType, (uint32_t)ARRAY_TYPE_INTEGER);
}

TEST("core/value refs: typed array helpers store and read elements") {
    ArrayOfFloat f(2);
    f.at(0, 1.5f);
    CHECK_EQ(f.at(0), 1.5f);

    ArrayOfDouble d(2);
    d.at(1, 2.25);
    CHECK_EQ(d.at(1), 2.25);

    ArrayOfBoolean b(1);
    b.at(0, true);
    CHECK(b.at(0));

    ArrayOfString s(1);
    s.at(0, "text");
    CHECK_STR(s.at(0), "text");

    ArrayOfInteger fromValue(Value::makeArrayRef(4, ARRAY_TYPE_INTEGER, 0));
    CHECK_EQ(fromValue.size(), (size_t)4);
    ArrayOfInteger notArray(Value(5));
    CHECK(!notArray);
}

TEST_BUG("core/value refs: ArrayOfFloat is a float array", "C12") {
    ArrayOfFloat arr(1);
    CHECK_EQ(arr.value.getArray()->arrayType, (uint32_t)ARRAY_TYPE_FLOAT);
}

TEST_BUG("core/value refs: ArrayOfDouble is a double array", "C12") {
    ArrayOfDouble arr(1);
    CHECK_EQ(arr.value.getArray()->arrayType, (uint32_t)ARRAY_TYPE_DOUBLE);
}

TEST_BUG("core/value refs: ArrayOfBoolean is a boolean array", "C12") {
    ArrayOfBoolean arr(1);
    CHECK_EQ(arr.value.getArray()->arrayType, (uint32_t)ARRAY_TYPE_BOOLEAN);
}

TEST_BUG("core/value refs: ArrayOfString is a string array", "C12") {
    ArrayOfString arr(1);
    CHECK_EQ(arr.value.getArray()->arrayType, (uint32_t)ARRAY_TYPE_STRING);
}
