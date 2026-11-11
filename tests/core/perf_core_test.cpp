/*
 * eez-framework tests: the core performance work (CP3, O16): one allocation
 * per string ref, move operations of Value, NaN is the same value for the
 * change detection, the derived unit table.
 */

#include <math.h>
#include <stddef.h>
#include <string.h>

#include <string>

#include <eez/conf-internal.h>
#include <eez/core/value.h>
#include <eez/core/unit.h>
#include <eez/flow/flow_defs_v3.h>

#include "test.h"

using namespace eez;
using namespace eez::flow::defs_v3;

// the allocation hooks of the sanitizer runtime (AddressSanitizer,
// ThreadSanitizer): not there in a build without sanitizers, then the
// allocations are not counted
extern "C" int __sanitizer_install_malloc_and_free_hooks(
    void (*mallocHook)(const volatile void *, size_t),
    void (*freeHook)(const volatile void *)) __attribute__((weak));

namespace {

bool g_counting;
int g_numAllocs;
int g_numFrees;

void mallocHook(const volatile void *, size_t) {
    if (g_counting) {
        g_numAllocs++;
    }
}

void freeHook(const volatile void *ptr) {
    if (g_counting && ptr) {
        g_numFrees++;
    }
}

// counts the allocations and frees of the code between start() and stop()
struct AllocCounter {
    bool available;

    AllocCounter() {
        static bool installed;
        if (!installed && __sanitizer_install_malloc_and_free_hooks) {
            installed = __sanitizer_install_malloc_and_free_hooks(mallocHook, freeHook) != 0;
        }
        available = installed;
    }

    void start() {
        g_numAllocs = 0;
        g_numFrees = 0;
        g_counting = true;
    }

    void stop() {
        g_counting = false;
    }
};

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

std::string text(const Value &value) {
    char buffer[128];
    value.toText(buffer, sizeof(buffer));
    return buffer;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// one allocation per string ref (O16)

TEST("core/perf: the allocation counter counts with the sanitizers") {
    AllocCounter counter;
    counter.start();
    void *ptr = eez::alloc(16, 0);
    eez::free(ptr);
    counter.stop();
    if (counter.available) {
        CHECK_EQ(g_numAllocs, 1);
        CHECK_EQ(g_numFrees, 1);
    }
}

TEST("core/perf: makeStringRef is one allocation, freed with the value") {
    AllocCounter counter;
    counter.start();
    {
        Value v = Value::makeStringRef("hello", -1, 0);
        CHECK_STR(v.getString(), "hello");
    }
    counter.stop();
    if (counter.available) {
        CHECK_EQ(g_numAllocs, 1);
        CHECK_EQ(g_numFrees, 1);
    }
}

TEST("core/perf: concatenateString is one allocation") {
    Value a("abc");
    Value b = Value::makeStringRef("defg", -1, 0);
    AllocCounter counter;
    counter.start();
    {
        Value c = Value::concatenateString(a, b);
        CHECK_STR(c.getString(), "abcdefg");
        CHECK_EQ(refCount(c), 1u);
    }
    counter.stop();
    if (counter.available) {
        CHECK_EQ(g_numAllocs, 1);
        CHECK_EQ(g_numFrees, 1);
    }
}

TEST("core/perf: concatenateString of empty strings") {
    Value c = Value::concatenateString(Value(""), Value(""));
    CHECK_EQ(c.getType(), VALUE_TYPE_STRING_REF);
    CHECK_STR(c.getString(), "");
    Value d = Value::concatenateString(Value("x"), Value(""));
    CHECK_STR(d.getString(), "x");
    Value e = Value::concatenateString(Value(""), Value("y"));
    CHECK_STR(e.getString(), "y");
}

TEST("core/perf: toString of a float is one allocation, toText none") {
    Value f(1.5f, UNIT_VOLT);
    AllocCounter counter;
    counter.start();
    {
        // toString of a float is "%g", without the unit
        Value s = f.toString(0);
        CHECK_STR(s.getString(), "1.5");
    }
    counter.stop();
    if (counter.available) {
        CHECK_EQ(g_numAllocs, 1);
        CHECK_EQ(g_numFrees, 1);
    }

    counter.start();
    std::string t;
    {
        char buffer[64];
        Value(0.0123f, UNIT_VOLT).toText(buffer, sizeof(buffer));
        counter.stop();
        t = buffer;
    }
    CHECK_EQ(t, std::string("12.3 mV"));
    if (counter.available) {
        CHECK_EQ(g_numAllocs, 0);
    }
}

TEST("core/perf: makeStringRefBuffer gives room for the string") {
    char *str = nullptr;
    Value v = Value::makeStringRefBuffer(5, 0, &str);
    REQUIRE(str != nullptr);
    CHECK_EQ(v.getType(), VALUE_TYPE_STRING_REF);
    CHECK_EQ((int)v.getOptions(), VALUE_OPTIONS_REF);
    CHECK_EQ(refCount(v), 1u);
    // empty until the caller writes it
    CHECK_STR(v.getString(), "");
    memcpy(str, "hello", 6);
    CHECK(v.getString() == str);
    CHECK_STR(v.getString(), "hello");

    Value copy = v;
    CHECK_EQ(refCount(v), 2u);
    CHECK_STR(copy.getString(), "hello");
}

TEST("core/perf: makeStringRefBuffer of an empty string") {
    char *str = nullptr;
    Value v = Value::makeStringRefBuffer(0, 0, &str);
    REQUIRE(str != nullptr);
    CHECK_STR(v.getString(), "");
    CHECK(!v.toBool());
}

TEST("core/perf: makeStringRefBuffer with a negative length fails like an allocation") {
    char *str = (char *)1;
    Value v = Value::makeStringRefBuffer(-1, 0, &str);
    CHECK(str == nullptr);
    CHECK_EQ(v.getType(), VALUE_TYPE_NULL);
    CHECK(!v.isString());

    // makeStringRef with a length below -1 (-1 is strlen)
    Value w = Value::makeStringRef("abc", -2, 0);
    CHECK_EQ(w.getType(), VALUE_TYPE_NULL);
}

TEST("core/perf: a string ref outlives the value it was copied from") {
    Value keep;
    {
        Value v = Value::makeStringRef("kept", -1, 0);
        keep = v;
        CHECK_EQ(refCount(keep), 2u);
    }
    CHECK_EQ(refCount(keep), 1u);
    CHECK_STR(keep.getString(), "kept");
}

////////////////////////////////////////////////////////////////////////////////
// move operations (CP3)

TEST("core/perf: the move constructor takes the reference, the source is undefined") {
    Value a = Value::makeStringRef("moved", -1, 0);
    Ref *ref = a.refValue;
    Value b(static_cast<Value &&>(a));
    CHECK(b.refValue == ref);
    CHECK_EQ(refCount(b), 1u);
    CHECK_STR(b.getString(), "moved");
    CHECK_EQ(a.getType(), VALUE_TYPE_UNDEFINED);
    CHECK_EQ((int)a.getOptions(), 0);
    CHECK(a == Value());
}

TEST("core/perf: the move constructor is not counted") {
    Value a = Value::makeArrayRef(2, ARRAY_TYPE_INTEGER, 0);
    AllocCounter counter;
    counter.start();
    Value b(static_cast<Value &&>(a));
    counter.stop();
    CHECK_EQ(refCount(b), 1u);
    if (counter.available) {
        CHECK_EQ(g_numAllocs, 0);
        CHECK_EQ(g_numFrees, 0);
    }
}

TEST("core/perf: move assignment releases the old value and takes the new one") {
    Value old = Value::makeStringRef("old", -1, 0);
    Value keepOld = old;
    CHECK_EQ(refCount(keepOld), 2u);

    Value a = Value::makeStringRef("new", -1, 0);
    old = static_cast<Value &&>(a);
    CHECK_EQ(refCount(keepOld), 1u);
    CHECK_STR(old.getString(), "new");
    CHECK_EQ(refCount(old), 1u);
    CHECK_EQ(a.getType(), VALUE_TYPE_UNDEFINED);
}

TEST("core/perf: assigning a temporary moves it") {
    Value v = Value::makeStringRef("first", -1, 0);
    v = Value::makeStringRef("second", -1, 0);
    CHECK_STR(v.getString(), "second");
    CHECK_EQ(refCount(v), 1u);
    v = Value(5);
    CHECK_EQ(v.getType(), VALUE_TYPE_INT32);
    CHECK_EQ(v.getInt(), 5);
}

TEST("core/perf: self move assignment keeps the value") {
    Value a = Value::makeStringRef("self", -1, 0);
    Value &alias = a;
    a = static_cast<Value &&>(alias);
    CHECK_STR(a.getString(), "self");
    CHECK_EQ(refCount(a), 1u);
}

TEST("core/perf: self copy assignment keeps the value") {
    Value a = Value::makeStringRef("self", -1, 0);
    Value &alias = a;
    a = alias;
    CHECK_STR(a.getString(), "self");
    CHECK_EQ(refCount(a), 1u);
}

TEST("core/perf: moving an element of the array into the array") {
    // the array is the only owner of the element: the element is taken before
    // the array is released
    Value arr = Value::makeArrayRef(1, ARRAY_TYPE_STRING, 0);
    arr.getArray()->values[0] = Value::makeStringRef("element", -1, 0);
    arr = static_cast<Value &&>(arr.getArray()->values[0]);
    CHECK_EQ(arr.getType(), VALUE_TYPE_STRING_REF);
    CHECK_STR(arr.getString(), "element");
    CHECK_EQ(refCount(arr), 1u);
}

TEST("core/perf: copying an element of the array into the array") {
    Value arr = Value::makeArrayRef(1, ARRAY_TYPE_STRING, 0);
    arr.getArray()->values[0] = Value::makeStringRef("element", -1, 0);
    arr = arr.getArray()->values[0];
    CHECK_STR(arr.getString(), "element");
    CHECK_EQ(refCount(arr), 1u);
}

TEST("core/perf: moving a string asset copies it, the asset is not changed") {
    StringAsset asset("asset");
    Value moved(static_cast<Value &&>(asset.value));
    CHECK_EQ(moved.getType(), VALUE_TYPE_STRING);
    CHECK(moved.getString() == asset.str);
    CHECK_EQ(asset.value.getType(), VALUE_TYPE_STRING_ASSET);
    CHECK_STR(asset.value.getString(), "asset");

    Value assigned = Value(1);
    assigned = static_cast<Value &&>(asset.value);
    CHECK_EQ(assigned.getType(), VALUE_TYPE_STRING);
    CHECK(assigned.getString() == asset.str);
    CHECK_EQ(asset.value.getType(), VALUE_TYPE_STRING_ASSET);
}

TEST("core/perf: the copy constructor keeps the fields") {
    Value src(2.5f, UNIT_VOLT, FLOAT_OPTIONS_LESS_THEN);
    src.dstValueType = VALUE_TYPE_DOUBLE;
    Value copy(src);
    CHECK_EQ(copy.getType(), VALUE_TYPE_FLOAT);
    CHECK_EQ(copy.getUnit(), UNIT_VOLT);
    CHECK_EQ((int)copy.getOptions(), FLOAT_OPTIONS_LESS_THEN);
    CHECK_EQ((int)copy.dstValueType, (int)VALUE_TYPE_DOUBLE);
    CHECK_EQ(copy.getFloat(), 2.5f);

    Value moved(static_cast<Value &&>(copy));
    CHECK_EQ(moved.getUnit(), UNIT_VOLT);
    CHECK_EQ((int)moved.getOptions(), FLOAT_OPTIONS_LESS_THEN);
    CHECK_EQ((int)moved.dstValueType, (int)VALUE_TYPE_DOUBLE);
    CHECK_EQ(moved.getFloat(), 2.5f);
}

////////////////////////////////////////////////////////////////////////////////
// NaN is the same value for the change detection (CP3)

TEST("core/perf: operator== takes a NaN as the same value as another NaN") {
    CHECK(Value(NAN, UNIT_VOLT) == Value(NAN, UNIT_VOLT));
    CHECK(Value(NAN, UNIT_VOLT) == Value(-NAN, UNIT_VOLT));
    CHECK(Value(NAN, VALUE_TYPE_FLOAT) == Value(NAN, VALUE_TYPE_FLOAT));
    CHECK(Value((double)NAN, VALUE_TYPE_DOUBLE) == Value((double)NAN, VALUE_TYPE_DOUBLE));
    CHECK(Value((double)NAN, VALUE_TYPE_DATE) == Value((double)NAN, VALUE_TYPE_DATE));
    CHECK(!(Value(NAN, UNIT_VOLT) != Value(NAN, UNIT_VOLT)));
}

TEST("core/perf: a NaN is not the same as a number, or a NaN with another unit or type") {
    CHECK(Value(NAN, UNIT_VOLT) != Value(1.0f, UNIT_VOLT));
    CHECK(Value(1.0f, UNIT_VOLT) != Value(NAN, UNIT_VOLT));
    CHECK(Value(NAN, UNIT_VOLT) != Value(NAN, UNIT_AMPER));
    CHECK(Value(NAN, UNIT_VOLT) != Value(NAN, UNIT_VOLT, FLOAT_OPTIONS_LESS_THEN));
    CHECK(Value(NAN, VALUE_TYPE_FLOAT) != Value((double)NAN, VALUE_TYPE_DOUBLE));
    CHECK(Value((double)NAN, VALUE_TYPE_DOUBLE) != Value(0.0, VALUE_TYPE_DOUBLE));
    CHECK(Value((double)NAN, VALUE_TYPE_DATE) != Value(0.0, VALUE_TYPE_DATE));
    // numbers compare as before
    CHECK(Value(1.5f, UNIT_VOLT) == Value(1.5f, UNIT_VOLT));
    CHECK(Value(0.0f, UNIT_VOLT) == Value(-0.0f, UNIT_VOLT));
    CHECK(Value(INFINITY, UNIT_VOLT) == Value(INFINITY, UNIT_VOLT));
    CHECK(Value(INFINITY, UNIT_VOLT) != Value(-INFINITY, UNIT_VOLT));
}

TEST("core/perf: a NaN through a value pointer is the same value") {
    Value nan1(NAN, UNIT_VOLT);
    Value nan2(NAN, UNIT_VOLT);
    CHECK(Value(&nan1, VALUE_TYPE_VALUE_PTR) == Value(&nan2, VALUE_TYPE_VALUE_PTR));
}

TEST("core/perf: isNaNNumber") {
    CHECK(Value(NAN, UNIT_VOLT).isNaNNumber());
    CHECK(Value((double)NAN, VALUE_TYPE_DOUBLE).isNaNNumber());
    CHECK(Value((double)NAN, VALUE_TYPE_DATE).isNaNNumber());
    CHECK(!Value(1.0f, UNIT_VOLT).isNaNNumber());
    CHECK(!Value(INFINITY, UNIT_VOLT).isNaNNumber());
    CHECK(!Value(0.0, VALUE_TYPE_DOUBLE).isNaNNumber());
    CHECK(!Value(5).isNaNNumber());
    CHECK(!Value("nan").isNaNNumber());
    CHECK(!Value().isNaNNumber());
}

////////////////////////////////////////////////////////////////////////////////
// the derived unit table (CP3): the same units as the scan of the units with
// the same base unit and factor

namespace {

static const int NUM_UNITS = UNIT_MICRO_AMPER_PP + 1;
static const float FACTORS[] = { 1E-12F, 1E-9F, 1E-6F, 1E-3F, 1E0F, 1E3F, 1E6F, 1E9F, 1E12F };
static const int NUM_FACTORS = sizeof(FACTORS) / sizeof(float);

// the definition: the first unit with the same base unit and the factor
Unit scanDerivedUnit(Unit unit, float factor) {
    if (unit == UNIT_UNKNOWN) {
        return UNIT_UNKNOWN;
    }
    for (int i = 0; i < NUM_UNITS; i++) {
        if (getBaseUnit((Unit)i) == getBaseUnit(unit) && getUnitFactor((Unit)i) == factor) {
            return (Unit)i;
        }
    }
    return UNIT_UNKNOWN;
}

Unit scanFindDerivedUnit(float value, Unit unit) {
    if (value == 0) {
        return unit;
    }
    for (int k = 1; FACTORS[k] <= 1.0F; k++) {
        if (value < FACTORS[k]) {
            Unit result = scanDerivedUnit(unit, FACTORS[k - 1]);
            if (result != UNIT_UNKNOWN) {
                return result;
            }
        }
    }
    for (int k = NUM_FACTORS - 1; FACTORS[k] != 1.0F; k--) {
        if (value >= FACTORS[k]) {
            Unit result = scanDerivedUnit(unit, FACTORS[k]);
            if (result != UNIT_UNKNOWN) {
                return result;
            }
        }
    }
    return unit;
}

Unit scanGetSmallerUnit(Unit unit, float min, float precision) {
    float factor = getUnitFactor(unit);
    if (precision <= factor || min <= factor) {
        float smaller = FACTORS[0];
        for (int k = NUM_FACTORS - 1; k > 0; k--) {
            if (FACTORS[k] < factor) {
                smaller = FACTORS[k];
                break;
            }
        }
        return scanDerivedUnit(unit, smaller);
    }
    return UNIT_UNKNOWN;
}

Unit scanGetBiggestUnit(Unit unit, float max) {
    for (int k = NUM_FACTORS - 1; k >= 0; k--) {
        if (max >= FACTORS[k]) {
            Unit result = scanDerivedUnit(unit, FACTORS[k]);
            if (result != UNIT_UNKNOWN) {
                return result;
            }
        }
    }
    return UNIT_UNKNOWN;
}

Unit scanGetSmallestUnit(Unit unit, float min, float precision) {
    for (int k = 0; k < NUM_FACTORS; k++) {
        if (precision <= FACTORS[k] || min <= FACTORS[k]) {
            Unit result = scanDerivedUnit(unit, FACTORS[k]);
            if (result != UNIT_UNKNOWN) {
                return result;
            }
        }
    }
    return UNIT_UNKNOWN;
}

void sampleValues(float *values, int &n) {
    n = 0;
    for (int e = -14; e <= 14; e++) {
        float v = powf(10.0f, (float)e);
        values[n++] = v;
        values[n++] = 0.5f * v;
        values[n++] = -v;
    }
    for (int k = 0; k < NUM_FACTORS; k++) {
        values[n++] = FACTORS[k];
        values[n++] = nextafterf(FACTORS[k], 0);
    }
    values[n++] = 0;
    values[n++] = NAN;
    values[n++] = INFINITY;
}

} // namespace

TEST("core/perf: findDerivedUnit is the scan of the units, for every unit") {
    float values[128];
    int n;
    sampleValues(values, n);
    int mismatches = 0;
    for (int u = 0; u <= NUM_UNITS; u++) {
        Unit unit = u == NUM_UNITS ? UNIT_UNKNOWN : (Unit)u;
        for (int i = 0; i < n; i++) {
            if (findDerivedUnit(values[i], unit) != scanFindDerivedUnit(values[i], unit)) {
                if (mismatches++ < 5) {
                    test::fail(__FILE__, __LINE__, test::format("findDerivedUnit(%g, %d)", (double)values[i], u));
                }
            }
            if (getBiggestUnit(unit, values[i]) != scanGetBiggestUnit(unit, values[i])) {
                if (mismatches++ < 5) {
                    test::fail(__FILE__, __LINE__, test::format("getBiggestUnit(%d, %g)", u, (double)values[i]));
                }
            }
            for (int j = 0; j < n; j++) {
                if (getSmallerUnit(unit, values[i], values[j]) != scanGetSmallerUnit(unit, values[i], values[j])) {
                    if (mismatches++ < 5) {
                        test::fail(__FILE__, __LINE__, test::format("getSmallerUnit(%d, %g, %g)", u, (double)values[i], (double)values[j]));
                    }
                }
                if (getSmallestUnit(unit, values[i], values[j]) != scanGetSmallestUnit(unit, values[i], values[j])) {
                    if (mismatches++ < 5) {
                        test::fail(__FILE__, __LINE__, test::format("getSmallestUnit(%d, %g, %g)", u, (double)values[i], (double)values[j]));
                    }
                }
            }
        }
    }
    CHECK_EQ(mismatches, 0);
}

TEST("core/perf: every factor of every unit is found like the scan") {
    // getBiggestUnit(unit, factor) with max = factor reaches the derived unit
    // of that factor first when there is one
    for (int u = 0; u < NUM_UNITS; u++) {
        for (int k = 0; k < NUM_FACTORS; k++) {
            Unit expected = scanDerivedUnit((Unit)u, FACTORS[k]);
            if (expected != UNIT_UNKNOWN) {
                CHECK_EQ((int)getBiggestUnit((Unit)u, FACTORS[k]), (int)expected);
                CHECK_EQ((int)getSmallestUnit((Unit)u, FACTORS[k], FACTORS[k]), (int)expected);
            }
        }
    }
}

TEST("core/perf: float text with derived units") {
    CHECK_EQ(text(Value(0.0123f, UNIT_VOLT)), "12.3 mV");
    CHECK_EQ(text(Value(0.0000005f, UNIT_AMPER)), "0.5 uA");
    CHECK_EQ(text(Value(2e-11f, UNIT_FARAD)), "20 pF");
    CHECK_EQ(text(Value(4700.0f, UNIT_OHM)), "4.7 K\xC2\xB4");
    CHECK_EQ(text(Value(2500000.0f, UNIT_HERTZ)), "2.5 MHz");
    CHECK_EQ(text(Value(5.0f, UNIT_VOLT)), "5.0 V");
    CHECK_EQ(text(Value(0.5f, UNIT_NONE)), "0.5");
}
