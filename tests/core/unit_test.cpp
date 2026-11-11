/*
 * eez-framework tests: core/unit (names, base units, factors, derived units)
 */

#include <string.h>

#include <eez/conf-internal.h>
#include <eez/core/unit.h>

#include "test.h"

using namespace eez;

TEST("core/unit: names") {
    CHECK_STR(getUnitName(UNIT_NONE), "");
    CHECK_STR(getUnitName(UNIT_UNKNOWN), "");
    CHECK_STR(getUnitName(UNIT_VOLT), "V");
    CHECK_STR(getUnitName(UNIT_MILLI_VOLT), "mV");
    CHECK_STR(getUnitName(UNIT_MICRO_AMPER), "uA");
    CHECK_STR(getUnitName(UNIT_MILLI_SECOND), "ms");
    CHECK_STR(getUnitName(UNIT_CELSIUS), DEGREE_SYMBOL "C");
    // U+00B4, the Ohm glyph of the fonts
    CHECK_STR(getUnitName(UNIT_OHM), "\xC2\xB4");
    CHECK_STR(getUnitName(UNIT_PERCENT), "%");
    CHECK_STR(getUnitName(UNIT_MHERTZ), "MHz");
    CHECK_STR(getUnitName(UNIT_PICO_FARAD), "pF");
    CHECK_STR(getUnitName(UNIT_VOLT_AMPERE_REACTIVE), "VAR");
    CHECK_STR(getUnitName(UNIT_DEGREE), DEGREE_SYMBOL);
    CHECK_STR(getUnitName(UNIT_MICRO_AMPER_PP), "uApp");
}

TEST_BUG("core/unit: kilo units are written with a lower case k", "CK3") {
    CHECK_STR(getUnitName(UNIT_KHERTZ), "kHz");
}

TEST("core/unit: getUnitFromName") {
    CHECK_EQ(getUnitFromName("mA"), UNIT_MILLI_AMPER);
    CHECK_EQ(getUnitFromName("V"), UNIT_VOLT);
    CHECK_EQ(getUnitFromName("uApp"), UNIT_MICRO_AMPER_PP);
    CHECK_EQ(getUnitFromName(""), UNIT_NONE);
    // unknown names and nullptr give UNIT_NONE
    CHECK_EQ(getUnitFromName("parsec"), UNIT_NONE);
    CHECK_EQ(getUnitFromName(nullptr), UNIT_NONE);
    // names are case sensitive
    CHECK_EQ(getUnitFromName("MV"), UNIT_NONE);
}

TEST("core/unit: every unit name maps back to its unit") {
    for (int unit = UNIT_NONE + 1; unit <= UNIT_MICRO_AMPER_PP; unit++) {
        if (getUnitFromName(getUnitName((Unit)unit)) != unit) {
            test::fail(__FILE__, __LINE__, test::format("unit %d (%s)", unit, getUnitName((Unit)unit)));
        }
    }
}

TEST("core/unit: base units") {
    CHECK_EQ(getBaseUnit(UNIT_MILLI_VOLT), UNIT_VOLT);
    CHECK_EQ(getBaseUnit(UNIT_VOLT), UNIT_VOLT);
    CHECK_EQ(getBaseUnit(UNIT_MICRO_AMPER), UNIT_AMPER);
    CHECK_EQ(getBaseUnit(UNIT_MILLI_WATT), UNIT_WATT);
    CHECK_EQ(getBaseUnit(UNIT_KOHM), UNIT_OHM);
    CHECK_EQ(getBaseUnit(UNIT_MHERTZ), UNIT_HERTZ);
    CHECK_EQ(getBaseUnit(UNIT_NANO_FARAD), UNIT_FARAD);
    CHECK_EQ(getBaseUnit(UNIT_MINUTE), UNIT_SECOND);
    CHECK_EQ(getBaseUnit(UNIT_VOLT_AMPERE_REACTIVE), UNIT_VOLT_AMPERE);
    CHECK_EQ(getBaseUnit(UNIT_MILLI_AMPER_PP), UNIT_AMPER_PP);
    CHECK_EQ(getBaseUnit(UNIT_UNKNOWN), UNIT_UNKNOWN);
}

TEST("core/unit: factors") {
    CHECK_EQ(getUnitFactor(UNIT_VOLT), 1.0f);
    CHECK_EQ(getUnitFactor(UNIT_MILLI_VOLT), 1E-3f);
    CHECK_EQ(getUnitFactor(UNIT_MICRO_AMPER), 1E-6f);
    CHECK_EQ(getUnitFactor(UNIT_KOHM), 1E3f);
    CHECK_EQ(getUnitFactor(UNIT_MOHM), 1E6f);
    CHECK_EQ(getUnitFactor(UNIT_PICO_FARAD), 1E-12f);
    CHECK_EQ(getUnitFactor(UNIT_MINUTE), 60.0f);
    CHECK_EQ(getUnitFactor(UNIT_UNKNOWN), 1.0f);
}

TEST("core/unit: findDerivedUnit picks a smaller unit for small values") {
    CHECK_EQ(findDerivedUnit(0.0005f, UNIT_VOLT), UNIT_MILLI_VOLT);
    CHECK_EQ(findDerivedUnit(0.5f, UNIT_AMPER), UNIT_MILLI_AMPER);
    CHECK_EQ(findDerivedUnit(5e-7f, UNIT_AMPER), UNIT_MICRO_AMPER);
    // no nA: the smallest unit below the value that exists
    CHECK_EQ(findDerivedUnit(2e-9f, UNIT_AMPER), UNIT_MICRO_AMPER);
    CHECK_EQ(findDerivedUnit(5e-11f, UNIT_FARAD), UNIT_PICO_FARAD);
    CHECK_EQ(findDerivedUnit(0.0047f, UNIT_SECOND), UNIT_MILLI_SECOND);
}

TEST("core/unit: findDerivedUnit picks a bigger unit for big values") {
    CHECK_EQ(findDerivedUnit(1500.0f, UNIT_OHM), UNIT_KOHM);
    CHECK_EQ(findDerivedUnit(2e6f, UNIT_OHM), UNIT_MOHM);
    CHECK_EQ(findDerivedUnit(2e9f, UNIT_OHM), UNIT_MOHM);
    CHECK_EQ(findDerivedUnit(1500.0f, UNIT_HERTZ), UNIT_KHERTZ);
    // no kV
    CHECK_EQ(findDerivedUnit(1500.0f, UNIT_VOLT), UNIT_VOLT);
}

TEST("core/unit: findDerivedUnit keeps the unit for 1 to 999 and 0") {
    CHECK_EQ(findDerivedUnit(1.0f, UNIT_VOLT), UNIT_VOLT);
    CHECK_EQ(findDerivedUnit(999.0f, UNIT_OHM), UNIT_OHM);
    CHECK_EQ(findDerivedUnit(0.0f, UNIT_MILLI_VOLT), UNIT_MILLI_VOLT);
    CHECK_EQ(findDerivedUnit(0.5f, UNIT_UNKNOWN), UNIT_UNKNOWN);
    CHECK_EQ(findDerivedUnit(0.5f, UNIT_PERCENT), UNIT_PERCENT);
}

TEST("core/unit: findDerivedUnit works from the base unit of a derived unit") {
    // pinned: the result is relative to the base unit, the value is not
    // converted (the value text uses it only for a value in a base unit, C1)
    CHECK_EQ(findDerivedUnit(0.5f, UNIT_MILLI_VOLT), UNIT_MILLI_VOLT);
    CHECK_EQ(findDerivedUnit(720.0f, UNIT_MINUTE), UNIT_MINUTE);
    CHECK_EQ(findDerivedUnit(1500.0f, UNIT_KOHM), UNIT_KOHM);
}

TEST("core/unit: getSmallerUnit") {
    CHECK_EQ(getSmallerUnit(UNIT_VOLT, 0.001f, 0.001f), UNIT_MILLI_VOLT);
    CHECK_EQ(getSmallerUnit(UNIT_AMPER, 0.5f, 0.5f), UNIT_MILLI_AMPER);
    CHECK_EQ(getSmallerUnit(UNIT_MILLI_AMPER, 0.0f, 1e-6f), UNIT_MICRO_AMPER);
    CHECK_EQ(getSmallerUnit(UNIT_VOLT, 5.0f, 5.0f), UNIT_UNKNOWN);
    CHECK_EQ(getSmallerUnit(UNIT_MILLI_VOLT, 0.0f, 1e-6f), UNIT_UNKNOWN);
}

TEST("core/unit: getBiggestUnit") {
    CHECK_EQ(getBiggestUnit(UNIT_OHM, 5e6f), UNIT_MOHM);
    CHECK_EQ(getBiggestUnit(UNIT_OHM, 5e3f), UNIT_KOHM);
    CHECK_EQ(getBiggestUnit(UNIT_VOLT, 10.0f), UNIT_VOLT);
    CHECK_EQ(getBiggestUnit(UNIT_VOLT, 0.5f), UNIT_MILLI_VOLT);
    CHECK_EQ(getBiggestUnit(UNIT_UNKNOWN, 10.0f), UNIT_UNKNOWN);
}

TEST("core/unit: getSmallestUnit") {
    CHECK_EQ(getSmallestUnit(UNIT_VOLT, 0.0001f, 0.0001f), UNIT_MILLI_VOLT);
    CHECK_EQ(getSmallestUnit(UNIT_AMPER, 1.0f, 1e-6f), UNIT_MICRO_AMPER);
    CHECK_EQ(getSmallestUnit(UNIT_OHM, 1000.0f, 1000.0f), UNIT_KOHM);
    CHECK_EQ(getSmallestUnit(UNIT_PERCENT, 0.5f, 0.5f), UNIT_PERCENT);
}
