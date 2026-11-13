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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include <eez/core/util.h>
#include <eez/core/value.h>
#include <eez/core/vars.h>

#include <eez/flow/flow.h>
#include <eez/flow/hooks.h>
#include <eez/flow/date.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>

#if defined(EEZ_DASHBOARD_API)
#include <eez/flow/dashboard_api.h>
#endif

namespace eez {

////////////////////////////////////////////////////////////////////////////////

static bool compare_UNDEFINED_value(const Value &a, const Value &b) {
    return b.type == VALUE_TYPE_UNDEFINED && a.int32Value == b.int32Value;
}

static void UNDEFINED_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    *text = 0;
}

static const char *UNDEFINED_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "undefined";
}

static bool compare_NULL_value(const Value &a, const Value &b) {
    EEZ_UNUSED(a);
    return b.type == VALUE_TYPE_NULL;
}

static void NULL_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    *text = 0;
}

static const char *NULL_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "null";
}

static bool compare_BOOLEAN_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getInt() == b.getInt();
}

static void BOOLEAN_value_to_text(const Value &value, char *text, int count) {
    if (value.getInt()) {
        stringCopy(text, count, "true");
    } else {
        stringCopy(text, count, "false");
    }
}

static const char *BOOLEAN_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "boolean";
}

static bool compare_INT8_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getInt8() == b.getInt8();
}

static void INT8_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt(text, count, value.getInt8());
}

static const char *INT8_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "int8";
}

static bool compare_UINT8_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUInt8() == b.getUInt8();
}

static void UINT8_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt32(text, count, value.getUInt8());
}

static const char *UINT8_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "uint8";
}

static bool compare_INT16_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getInt16() == b.getInt16();
}

static void INT16_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt(text, count, value.getInt16());
}

static const char *INT16_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "int16";
}

static bool compare_UINT16_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUInt16() == b.getUInt16();
}

static void UINT16_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt32(text, count, value.getUInt16());
}

static const char *UINT16_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "uint16";
}

static bool compare_INT32_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getInt32() == b.getInt32();
}

static void INT32_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt(text, count, value.getInt32());
}

static const char *INT32_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "int32";
}

static bool compare_UINT32_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUInt32() == b.getUInt32();
}

static void UINT32_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt32(text, count, value.getUInt32());
}

static const char *UINT32_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "uint32";
}

static bool compare_INT64_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getInt64() == b.getInt64();
}

static void INT64_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt64(text, count, value.getInt64());
}

static const char *INT64_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "int64";
}

static bool compare_UINT64_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUInt64() == b.getUInt64();
}

static void UINT64_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt64(text, count, value.getUInt64());
}

static const char *UINT64_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "uint64";
}

// a NaN is the same as another NaN (see Value::operator==)
static bool compare_FLOAT_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUnit() == b.getUnit() && a.getOptions() == b.getOptions() &&
        (a.getFloat() == b.getFloat() || (isNaN(a.getFloat()) && isNaN(b.getFloat())));
}

static void FLOAT_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;

    float floatValue = value.getFloat();

#if defined(INFINITY_SYMBOL)
    if (isinf(floatValue)) {
        snprintf(text, count, INFINITY_SYMBOL);
        return;
    }
#endif

    Unit unit = value.getUnit();

    bool appendDotZero = unit == UNIT_VOLT || unit == UNIT_VOLT_PP || unit == UNIT_AMPER || unit == UNIT_AMPER_PP || unit == UNIT_WATT;

    uint16_t options = value.getOptions();
    bool fixedDecimals = (options & FLOAT_OPTIONS_FIXED_DECIMALS) != 0;

    if (floatValue != 0) {
        // a value in a base unit is shown in the derived unit that fits it
        // (0.005 V as 5 mV), a value in another unit keeps its unit
        if (!fixedDecimals && getBaseUnit(unit) == unit) {
            unit = findDerivedUnit(fabsf(floatValue), unit);
            floatValue /= getUnitFactor(unit);
        }
    } else {
        floatValue = 0; // set to zero just in case we have negative zero
    }

    if (!isNaN(floatValue)) {
        if ((value.getOptions() & FLOAT_OPTIONS_LESS_THEN) != 0) {
            stringAppendString(text, count, "< ");
			appendDotZero = false;
        }

        if (fixedDecimals) {
            stringAppendFloat(text, count, floatValue, FLOAT_OPTIONS_GET_NUM_FIXED_DECIMALS(options));
        } else {
            if (unit == UNIT_WATT || unit == UNIT_MILLI_WATT) {
                stringAppendFloat(text, count, floatValue, 2);
            } else {
                stringAppendFloat(text, count, floatValue);
            }

            int n = (int)strlen(text);

            int decimalPointIndex;
            for (decimalPointIndex = 0; decimalPointIndex < n; ++decimalPointIndex) {
                if (text[decimalPointIndex] == '.') {
                    break;
                }
            }

            if (decimalPointIndex == n) {
                if (appendDotZero) {
                    // 1 => 1.0
                    stringAppendString(text, count, ".0");
                }
            } else if (decimalPointIndex == n - 1) {
                if (appendDotZero) {
                    // 1. => 1.0
                    stringAppendString(text, count, "0");
                } else {
                    text[decimalPointIndex] = 0;
                }
            } else {
                // remove trailing zeros
                if (appendDotZero) {
                    for (int j = n - 1; j > decimalPointIndex + 1 && text[j] == '0'; j--) {
                        text[j] = 0;
                    }
                } else {
                    for (int j = n - 1; j >= decimalPointIndex && (text[j] == '0' || text[j] == '.'); j--) {
                        text[j] = 0;
                    }
                }
            }
        }

        const char *unitName = getUnitName(unit);
        if (unitName && *unitName) {
            stringAppendString(text, count, " ");
            stringAppendString(text, count, unitName);
        }
    } else {
        text[0] = 0;
    }
}

static const char *FLOAT_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "float";
}

// a NaN is the same as another NaN (see Value::operator==)
static bool compare_DOUBLE_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUnit() == b.getUnit() && a.getOptions() == b.getOptions() &&
        (a.getDouble() == b.getDouble() || (isNaN(a.getDouble()) && isNaN(b.getDouble())));
}

static void DOUBLE_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;

    double doubleValue = value.getDouble();

#if defined(INFINITY_SYMBOL)
    if (isinf(doubleValue)) {
        snprintf(text, count, INFINITY_SYMBOL);
        return;
    }
#endif

    Unit unit = value.getUnit();

    bool appendDotZero = unit == UNIT_VOLT || unit == UNIT_VOLT_PP || unit == UNIT_AMPER || unit == UNIT_AMPER_PP || unit == UNIT_WATT;

    uint16_t options = value.getOptions();
    bool fixedDecimals = (options & FLOAT_OPTIONS_FIXED_DECIMALS) != 0;

    if (doubleValue != 0) {
        // a value in a base unit is shown in the derived unit that fits it
        // (0.005 V as 5 mV), a value in another unit keeps its unit
        if (!fixedDecimals && getBaseUnit(unit) == unit) {
            unit = findDerivedUnit(fabs(doubleValue), unit);
            doubleValue /= getUnitFactor(unit);
        }
    } else {
        doubleValue = 0; // set to zero just in case we have negative zero
    }

    if (!isNaN(doubleValue)) {
        if ((value.getOptions() & FLOAT_OPTIONS_LESS_THEN) != 0) {
            stringAppendString(text, count, "< ");
			appendDotZero = false;
        }

        if (fixedDecimals) {
            stringAppendFloat(text, count, doubleValue, FLOAT_OPTIONS_GET_NUM_FIXED_DECIMALS(options));
        } else {
            if (unit == UNIT_WATT || unit == UNIT_MILLI_WATT) {
                stringAppendDouble(text, count, doubleValue, 2);
            } else {
                stringAppendDouble(text, count, doubleValue);
            }

            int n = (int)strlen(text);

            int decimalPointIndex;
            for (decimalPointIndex = 0; decimalPointIndex < n; ++decimalPointIndex) {
                if (text[decimalPointIndex] == '.') {
                    break;
                }
            }

            if (decimalPointIndex == n) {
                if (appendDotZero) {
                    // 1 => 1.0
                    stringAppendString(text, count, ".0");
                }
            } else if (decimalPointIndex == n - 1) {
                if (appendDotZero) {
                    // 1. => 1.0
                    stringAppendString(text, count, "0");
                } else {
                    text[decimalPointIndex] = 0;
                }
            } else {
                // remove trailing zeros
                if (appendDotZero) {
                    for (int j = n - 1; j > decimalPointIndex + 1 && text[j] == '0'; j--) {
                        text[j] = 0;
                    }
                } else {
                    for (int j = n - 1; j >= decimalPointIndex && (text[j] == '0' || text[j] == '.'); j--) {
                        text[j] = 0;
                    }
                }
            }
        }

        const char *unitName = getUnitName(unit);
        if (unitName && *unitName) {
            stringAppendString(text, count, " ");
            stringAppendString(text, count, unitName);
        }
    } else {
        text[0] = 0;
    }
}

static const char *DOUBLE_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "double";
}

static bool compare_STRING_value(const Value &a, const Value &b) {
    if (!b.isString()) {
        return false;
    }
    const char *astr = a.getString();
    const char *bstr = b.getString();
    if (!astr && !bstr) {
        return true;
    }
    if ((!astr && bstr) || (astr && !bstr)) {
        return false;
    }
    return strcmp(astr, bstr) == 0;
}

static void STRING_value_to_text(const Value &value, char *text, int count) {
    const char *str = value.getString();
    if (str) {
        stringCopy(text, count, str);
    } else {
        text[0] = 0;
    }
}

static const char *STRING_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "string";
}

static bool compare_STRING_ASSET_value(const Value &a, const Value &b) {
    return compare_STRING_value(a, b);
}

static void STRING_ASSET_value_to_text(const Value &value, char *text, int count) {
    STRING_value_to_text(value, text, count);
}

static const char *STRING_ASSET_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "string";
}

static bool compare_ARRAY_value(const Value &a, const Value &b) {
    return a.type == b.type && a.arrayValue == b.arrayValue;
}

static void ARRAY_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *ARRAY_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "array";
}

static bool compare_ARRAY_ASSET_value(const Value &a, const Value &b) {
    return a.type == b.type && a.int32Value == b.int32Value;
}

static void ARRAY_ASSET_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *ARRAY_ASSET_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "array";
}

static bool compare_ARRAY_REF_value(const Value &a, const Value &b) {
    return a.type == b.type && a.refValue == b.refValue;
}

static void ARRAY_REF_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *ARRAY_REF_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "array";
}

static bool compare_DYNAMIC_ARRAY_value(const Value &a, const Value &b) {
    return a.type == b.type && a.pVoidValue == b.pVoidValue;
}

static void DYNAMIC_ARRAY_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *DYNAMIC_ARRAY_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "array";
}

static bool compare_STRING_REF_value(const Value &a, const Value &b) {
	return compare_STRING_value(a, b);
}

static void STRING_REF_value_to_text(const Value &value, char *text, int count) {
	STRING_value_to_text(value, text, count);
}

static const char *STRING_REF_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "string";
}

static bool compare_BLOB_REF_value(const Value &a, const Value &b) {
    return a.type == b.type && a.refValue == b.refValue;
}

static void BLOB_REF_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "blob (size=%d)", value.getInt());
}

static const char *BLOB_REF_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "blob";
}

static bool compare_STREAM_value(const Value &a, const Value &b) {
    return a.type == b.type && a.int32Value == b.int32Value;
}

static void STREAM_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "stream (id=%d)", value.getInt());
}

static const char *STREAM_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "stream";
}

static bool compare_WIDGET_value(const Value &a, const Value &b) {
#if defined(EEZ_FOR_LVGL)
    return a.type == b.type && a.getVoidPointer() == b.getVoidPointer();
#else
    return a.type == b.type && a.int32Value == b.int32Value;
#endif
}

static void WIDGET_value_to_text(const Value &value, char *text, int count) {
#if defined(EEZ_FOR_LVGL)
    snprintf(text, count, "widget (%p)", value.getVoidPointer());
#else
    snprintf(text, count, "widget (id=%d)", value.getInt());
#endif
}

static const char *WIDGET_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "widget";
}

static bool compare_JSON_value(const Value &a, const Value &b) {
    return a.type == b.type && a.int32Value == b.int32Value;
}

static void JSON_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "json (id=%d)", value.getInt());
}

static const char *JSON_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "json";
}

static bool compare_JSON_MEMBER_VALUE_value(const Value &a, const Value &b) {
	return a.getValue() == b.getValue();
}

static void JSON_MEMBER_VALUE_value_to_text(const Value &value, char *text, int count) {
	value.getValue().toText(text, count);
}

static const char *JSON_MEMBER_VALUE_value_type_name(const Value &value) {
    auto value2 = value.getValue();
    return g_valueTypeNames[value2.type](value2);
}

static bool compare_EVENT_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getVoidPointer() == b.getVoidPointer();
}

static void EVENT_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "event (%p)", value.getVoidPointer());
}

static const char *EVENT_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "event";
}

static const char *PROPERTY_REF_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "property-ref";
}

static bool compare_PROPERTY_REF_value(const Value &a, const Value &b) {
    return a.type == b.type && a.refValue == b.refValue;
}

static void PROPERTY_REF_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "property-ref (flowState=%p, component=%d, property=%d)",
        (void *)value.getPropertyRef()->flowState, value.getPropertyRef()->componentIndex, value.getPropertyRef()->propertyIndex);
}

// an invalid date (NaN) is the same as another invalid date (see Value::operator==)
static bool compare_DATE_value(const Value &a, const Value &b) {
    return a.type == b.type && (a.doubleValue == b.doubleValue || (isNaN(a.doubleValue) && isNaN(b.doubleValue)));
}

static void DATE_value_to_text(const Value &value, char *text, int count) {
    flow::date::toLocaleString((flow::date::Date)value.getDouble(), text, count);
}

static const char *DATE_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "date";
}

static bool compare_VERSIONED_STRING_value(const Value &a, const Value &b) {
    return a.type == b.type && a.unit == b.unit; // here unit is used as string version
}

static void VERSIONED_STRING_value_to_text(const Value &value, char *text, int count) {
    const char *str = value.getString();
    if (str) {
        stringCopy(text, count, str);
    } else {
        text[0] = 0;
    }
}

static const char *VERSIONED_STRING_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "versioned-string";
}

static bool compare_VALUE_PTR_value(const Value &a, const Value &b) {
	return a.type == b.type && (a.pValueValue == b.pValueValue || (a.pValueValue && b.pValueValue && *a.pValueValue == *b.pValueValue));
}

static void VALUE_PTR_value_to_text(const Value &value, char *text, int count) {
	if (value.pValueValue) {
		value.pValueValue->toText(text, count);
	} else {
		text[0] = 0;
	}
}

static const char *VALUE_PTR_value_type_name(const Value &value) {
	if (value.pValueValue) {
		return g_valueTypeNames[value.pValueValue->type](value.pValueValue);
	} else {
		return "null";
	}
}

static bool compare_ARRAY_ELEMENT_VALUE_value(const Value &a, const Value &b) {
	return a.getValue() == b.getValue();
}

static void ARRAY_ELEMENT_VALUE_value_to_text(const Value &value, char *text, int count) {
	value.getValue().toText(text, count);
}

static const char *ARRAY_ELEMENT_VALUE_value_type_name(const Value &value) {
    auto value2 = value.getValue();
    return g_valueTypeNames[value2.type](value2);
}

static bool compare_FLOW_OUTPUT_value(const Value &a, const Value &b) {
	return a.type == b.type && a.getUInt16() == b.getUInt16();
}

static void FLOW_OUTPUT_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *FLOW_OUTPUT_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "internal";
}

#if EEZ_OPTION_GUI
using namespace gui;

// getValue() reads the native variable (from the BB3+ assets) or the DIB field
// (with getModuleFieldHook and the slot index in the upper 16 bits of the id)

static bool compare_NATIVE_VARIABLE_value(const Value &a, const Value &b) {
    return a.getValue() == b.getValue();
}

static void NATIVE_VARIABLE_value_to_text(const Value &value, char *text, int count) {
    value.getValue().toText(text, count);
}

static const char *NATIVE_VARIABLE_value_type_name(const Value &value) {
    auto aValue = value.getValue();
    return g_valueTypeNames[aValue.type](aValue);
}

static bool compare_MODULE_FIELD_value(const Value &a, const Value &b) {
    return a.getValue() == b.getValue();
}

static void MODULE_FIELD_value_to_text(const Value &value, char *text, int count) {
    value.getValue().toText(text, count);
}

static const char *MODULE_FIELD_value_type_name(const Value &value) {
    auto aValue = value.getValue();
    return g_valueTypeNames[aValue.type](aValue);
}
#else

static bool compare_NATIVE_VARIABLE_value(const Value &a, const Value &b) {
    EEZ_UNUSED(a);
    EEZ_UNUSED(b);
    return false;
}

static void NATIVE_VARIABLE_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    *text = 0;
}

static const char *NATIVE_VARIABLE_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "";
}

static bool compare_MODULE_FIELD_value(const Value &a, const Value &b) {
    EEZ_UNUSED(a);
    EEZ_UNUSED(b);
    return false;
}

static void MODULE_FIELD_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    *text = 0;
}

static const char *MODULE_FIELD_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "";
}

#endif

static bool compare_ERROR_value(const Value &a, const Value &b) {
    EEZ_UNUSED(a);
    EEZ_UNUSED(b);
	return false;
}

static void ERROR_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    *text = 0;
}

static const char *ERROR_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "error";
}

static bool compare_RANGE_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUInt32() == b.getUInt32();
}

static void RANGE_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *RANGE_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "internal";
}

static bool compare_POINTER_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getVoidPointer() == b.getVoidPointer();
}

static void POINTER_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *POINTER_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "internal";
}

#if EEZ_OPTION_GUI
using namespace gui;
static bool compare_ENUM_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getEnum().enumDefinition == b.getEnum().enumDefinition &&
           a.getEnum().enumValue == b.getEnum().enumValue;
}

static void ENUM_value_to_text(const Value &value, char *text, int count) {
    const EnumItem *enumDefinition = g_enumDefinitions[value.getEnum().enumDefinition];
    for (int i = 0; enumDefinition[i].menuLabel; ++i) {
        if (value.getEnum().enumValue == enumDefinition[i].value) {
            if (enumDefinition[i].widgetLabel) {
                stringCopy(text, count, enumDefinition[i].widgetLabel);
            } else {
                stringCopy(text, count, enumDefinition[i].menuLabel);
            }
            break;
        }
    }
}

static const char *ENUM_value_type_name(const Value &value) {
    return "internal";
}
#else

static bool compare_ENUM_value(const Value &a, const Value &b) {
    EEZ_UNUSED(a);
    EEZ_UNUSED(b);
    return false;
}

static void ENUM_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    *text = 0;
}

static const char *ENUM_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "internal";
}

#endif // EEZ_OPTION_GUI

static bool compare_YT_DATA_GET_VALUE_FUNCTION_POINTER_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUInt32() == b.getUInt32();
}

static bool compare_IP_ADDRESS_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getUInt32() == b.getUInt32();
}

static void IP_ADDRESS_value_to_text(const Value &value, char *text, int count) {
    ipAddressToString(value.getUInt32(), text, count);
}

static const char *IP_ADDRESS_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "internal";
}

static bool compare_TIME_ZONE_value(const Value &a, const Value &b) {
    return a.type == b.type && a.getInt16() == b.getInt16();
}

static void TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    formatTimeZone(value.getInt16(), text, count);
}

static const char *TIME_ZONE_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "internal";
}

static void YT_DATA_GET_VALUE_FUNCTION_POINTER_value_to_text(const Value &value, char *text, int count) {
    EEZ_UNUSED(value);
    EEZ_UNUSED(count);
    text[0] = 0;
}

static const char *YT_DATA_GET_VALUE_FUNCTION_POINTER_value_type_name(const Value &value) {
    EEZ_UNUSED(value);
    return "internal";
}

////////////////////////////////////////////////////////////////////////////////

#define VALUE_TYPE(NAME) bool compare_##NAME##_value(const Value &a, const Value &b);
VALUE_TYPES
#undef VALUE_TYPE
#define VALUE_TYPE(NAME) compare_##NAME##_value,
CompareValueFunction g_valueTypeCompareFunctions[] = {
	VALUE_TYPES
};
#undef VALUE_TYPE

#define VALUE_TYPE(NAME) void NAME##_value_to_text(const Value &value, char *text, int count);
VALUE_TYPES
#undef VALUE_TYPE
#define VALUE_TYPE(NAME) NAME##_value_to_text,
ValueToTextFunction g_valueTypeToTextFunctions[] = {
	VALUE_TYPES
};
#undef VALUE_TYPE

#define VALUE_TYPE(NAME) const char * NAME##_value_type_name(const Value &value);
VALUE_TYPES
#undef VALUE_TYPE
#define VALUE_TYPE(NAME) NAME##_value_type_name,
ValueTypeNameFunction g_valueTypeNames[] = {
	VALUE_TYPES
};
#undef VALUE_TYPE

////////////////////////////////////////////////////////////////////////////////

ArrayValueRef::~ArrayValueRef() {
    eez::flow::onArrayValueFree(&arrayValue);
    for (uint32_t i = 1; i < arrayValue.arraySize; i++) {
        (arrayValue.values + i)->~Value();
    }
}

////////////////////////////////////////////////////////////////////////////////

bool assignValue(Value &dstValue, const Value &srcValue, uint32_t dstValueType) {
    // if (srcValue.type != dstValue.type) {
    //     printf("%s (%d) <- %s (%d)\n", dstValue.toString(0).getString(), dstValue.type, srcValue.toString(0).getString(), srcValue.type);
    // }

    if (dstValueType == VALUE_TYPE_BOOLEAN) {
        dstValue = Value(srcValue.toBool(), VALUE_TYPE_BOOLEAN);
    } else if (Value::isInt32OrLess(dstValueType)) {
        dstValue = Value((int)srcValue.toInt32(), (ValueType)dstValueType);
    } else if (dstValueType == VALUE_TYPE_FLOAT) {
        dstValue = Value(srcValue.toFloat(), VALUE_TYPE_FLOAT);
    } else if (dstValueType == VALUE_TYPE_DOUBLE) {
        dstValue = Value(srcValue.toDouble(), VALUE_TYPE_DOUBLE);
    } else if (dstValueType == VALUE_TYPE_STRING) {
        dstValue = srcValue.toString(0x30a91156);
#if defined(EEZ_DASHBOARD_API)
    } else if (dstValueType == VALUE_TYPE_JSON) {
        if (srcValue.isJson()) {
            dstValue = srcValue;
        } else {
            dstValue = flow::convertToJson(&srcValue);
        }
    } else if (srcValue.isJson()) {
        if (dstValueType == VALUE_TYPE_JSON) {
            dstValue = srcValue;
        }  else {
            dstValue = flow::convertFromJson(srcValue.getInt(), dstValueType);
        }
#endif
    } else if (dstValue.isBoolean()) {
        dstValue.int32Value = srcValue.toBool();
    } else if (dstValue.isInt32OrLess()) {
        dstValue.int32Value = srcValue.toInt32();
    } else if (dstValue.isFloat()) {
        dstValue.floatValue = srcValue.toFloat();
    } else if (dstValue.isDouble()) {
        dstValue.doubleValue = srcValue.toDouble();
    } else if (dstValue.isString()) {
        dstValue = srcValue.toString(0x30a91156);
    } else {
        dstValue = srcValue;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

uint16_t getPageIndexFromValue(const Value &value) {
    return value.getFirstUInt16();
}

uint16_t getNumPagesFromValue(const Value &value) {
    return value.getSecondUInt16();
}

////////////////////////////////////////////////////////////////////////////////

Value MakeRangeValue(uint16_t from, uint16_t to) {
    Value value;
    value.type = VALUE_TYPE_RANGE;
    value.pairOfUint16Value.first = from;
    value.pairOfUint16Value.second = to;
    return value;
}

Value MakeEnumDefinitionValue(uint8_t enumValue, uint8_t enumDefinition) {
    Value value;
    value.type = VALUE_TYPE_ENUM;
    value.enumValue.enumValue = enumValue;
    value.enumValue.enumDefinition = enumDefinition;
    return value;
}

////////////////////////////////////////////////////////////////////////////////

Value Value::getValue() const {
    if (type == VALUE_TYPE_VALUE_PTR) {
        return pValueValue->getValue();
    }
    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
#if EEZ_OPTION_GUI
        using namespace gui;
        Assets *temp = g_widgetCursor.assets;
        g_widgetCursor.assets = g_mainAssets;
        auto result = get(g_widgetCursor, int32Value);
        g_widgetCursor.assets = temp;
        return result;
#else
        return getVar(int32Value);
#endif
    }
    if (type == VALUE_TYPE_MODULE_FIELD) {
        using namespace flow;
        return getModuleFieldHook(uint32Value);
    }
    if (type == VALUE_TYPE_ARRAY_ELEMENT_VALUE) {
        auto arrayElementValue = (ArrayElementValue *)refValue;
        if (arrayElementValue->arrayValue.isBlob()) {
            auto blobRef = arrayElementValue->arrayValue.getBlob();
            if (arrayElementValue->elementIndex < 0 || arrayElementValue->elementIndex >= (int)blobRef->len) {
                return Value();
            }
            return Value((uint32_t)blobRef->blob[arrayElementValue->elementIndex], VALUE_TYPE_UINT32);
        } else if (arrayElementValue->arrayValue.isDynamicArray()) {
            auto dynamicArray = arrayElementValue->arrayValue.getDynamicArray();
            if (arrayElementValue->elementIndex < 0 || arrayElementValue->elementIndex >= (int)dynamicArray->arraySize) {
                return Value();
            }
            return dynamicArray->getElement(arrayElementValue->elementIndex);
        } else {
            auto array = arrayElementValue->arrayValue.getArray();

            if (arrayElementValue->elementIndex < 0 || arrayElementValue->elementIndex >= (int)array->arraySize) {
                return Value();
            }

#if defined(EEZ_DASHBOARD_API)
            if (array->arrayType >= flow::defs_v3::FIRST_OBJECT_TYPE && array->arrayType <= flow::defs_v3::LAST_OBJECT_TYPE) {
                return flow::getObjectVariableMemberValue(&arrayElementValue->arrayValue, arrayElementValue->elementIndex);
            }
#endif

            return array->values[arrayElementValue->elementIndex];
        }
    }

#if defined(EEZ_DASHBOARD_API)
    else if (type == VALUE_TYPE_JSON_MEMBER_VALUE) {
        auto jsonMemberValue = (JsonMemberValue *)refValue;
        return flow::operationJsonGet(jsonMemberValue->jsonValue.getInt(), jsonMemberValue->propertyName.getString());
    }
#endif

    else if (type == VALUE_TYPE_PROPERTY_REF) {
        return evalProperty();
    }

    return *this;
}

const char *Value::getString() const {
    // the string of this value or of a value stored in a variable or an array
    // element is read where it is, without a temporary copy
    if (type == VALUE_TYPE_STRING) {
        return strValue;
    }
	if (type == VALUE_TYPE_STRING_REF) {
		return ((StringRef *)refValue)->str;
	}
    if (type == VALUE_TYPE_STRING_ASSET) {
        return (const char *)((uint8_t *)&int32Value + int32Value);
    }
    if (type == VALUE_TYPE_VALUE_PTR) {
        return pValueValue ? pValueValue->getString() : nullptr;
    }
    if (type == VALUE_TYPE_ARRAY_ELEMENT_VALUE) {
        auto arrayElementValue = (ArrayElementValue *)refValue;
        if (arrayElementValue->arrayValue.isBlob()) {
            // an element of a blob is a number
            return nullptr;
        }
        auto array = arrayElementValue->arrayValue.getArray();
        bool isStoredElement = array != nullptr;
#if defined(EEZ_DASHBOARD_API)
        if (isStoredElement && array->arrayType >= flow::defs_v3::FIRST_OBJECT_TYPE && array->arrayType <= flow::defs_v3::LAST_OBJECT_TYPE) {
            isStoredElement = false;
        }
#endif
        if (isStoredElement) {
            if (arrayElementValue->elementIndex < 0 || arrayElementValue->elementIndex >= (int)array->arraySize) {
                return nullptr;
            }
            return array->values[arrayElementValue->elementIndex].getString();
        }
    }

    if (isIndirectValueType()) {
        // A computed value (native variable, module field, property ref, element
        // of a dynamic array or of a dashboard object, JSON member) can be a new
        // string that only the temporary result owns, so the caller resolves it
        // with getValue() and keeps the result while it uses the string.
        assert(!"getString() of a computed value, resolve it with getValue() first");

        auto value = getValue();
        if (value.type == VALUE_TYPE_STRING) {
            // a static string or a string in the assets
            return value.strValue;
        }
        if (value.type == VALUE_TYPE_STRING_REF && value.refValue->refCounter > 1) {
            // the string is also owned by something else, it stays after the
            // temporary is freed
            return ((StringRef *)value.refValue)->str;
        }
    }

	return nullptr;
}

const ArrayValue *Value::getArray() const {
    if (type == VALUE_TYPE_ARRAY) {
        return arrayValue;
    }
    if (type == VALUE_TYPE_ARRAY_ASSET) {
        return (ArrayValue *)((uint8_t *)&int32Value + int32Value);
    }
    if (type == VALUE_TYPE_ARRAY_REF) {
        return &((ArrayValueRef *)refValue)->arrayValue;
    }
    return nullptr;
}

ArrayValue *Value::getArray() {
    if (type == VALUE_TYPE_ARRAY) {
        return arrayValue;
    }
    if (type == VALUE_TYPE_ARRAY_ASSET) {
        return (ArrayValue *)((uint8_t *)&int32Value + int32Value);
    }
    if (type == VALUE_TYPE_ARRAY_REF) {
        return &((ArrayValueRef *)refValue)->arrayValue;
    }
    return nullptr;
}

double Value::toDouble(int *err) const {
	if (isIndirectValueType()) {
		return getValue().toDouble(err);
	}

	if (err) {
		*err = 0;
	}

	if (type == VALUE_TYPE_DOUBLE) {
		return doubleValue;
	}

	if (type == VALUE_TYPE_FLOAT) {
		return floatValue;
	}

	if (type == VALUE_TYPE_INT8) {
		return int8Value;
	}
	if (type == VALUE_TYPE_UINT8) {
		return uint8Value;
	}

	if (type == VALUE_TYPE_INT16) {
		return int16Value;
	}
	if (type == VALUE_TYPE_UINT16) {
		return uint16Value;
	}

	if (type == VALUE_TYPE_INT32 || type == VALUE_TYPE_BOOLEAN) {
		return int32Value;
	}
	if (type == VALUE_TYPE_UINT32) {
		return uint32Value;
	}

	if (type == VALUE_TYPE_INT64) {
		return (double)int64Value;
	}
	if (type == VALUE_TYPE_UINT64) {
		return (double)uint64Value;
	}

	if (type == VALUE_TYPE_DATE) {
		return doubleValue;
	}

	if (isString()) {
        const char *pStart = getString();
        char *pEnd;
		double value = strtod(pStart, &pEnd);
        while (isspace(*pEnd)) {
            pEnd++;
        }
        if (*pEnd == '\0') {
            return value;
        }
	}

    if (err) {
        *err = 1;
    }
	return NAN;
}

float Value::toFloat(int *err) const {
	if (isIndirectValueType()) {
		return getValue().toFloat(err);
	}

	if (err) {
		*err = 0;
	}

	if (type == VALUE_TYPE_DOUBLE) {
		return (float)doubleValue;
	}

	if (type == VALUE_TYPE_FLOAT) {
		return floatValue;
	}

	if (type == VALUE_TYPE_INT8) {
		return int8Value;
	}
	if (type == VALUE_TYPE_UINT8) {
		return uint8Value;
	}

	if (type == VALUE_TYPE_INT16) {
		return int16Value;
	}
	if (type == VALUE_TYPE_UINT16) {
		return uint16Value;
	}

	if (type == VALUE_TYPE_INT32 || type == VALUE_TYPE_BOOLEAN) {
		return (float)int32Value;
	}

	if (type == VALUE_TYPE_UINT32) {
		return (float)uint32Value;
	}

	if (type == VALUE_TYPE_INT64) {
		return (float)int64Value;
	}
	if (type == VALUE_TYPE_UINT64) {
		return (float)uint64Value;
	}

	if (isString()) {
        const char *pStart = getString();
        char *pEnd;
		float value = strtof(pStart, &pEnd);
        while (isspace(*pEnd)) {
            pEnd++;
        }
        if (*pEnd == '\0') {
            return value;
        }
	}

    if (err) {
        *err = 1;
    }
    return NAN;
}

int32_t Value::toInt32(int *err) const {
	if (isIndirectValueType()) {
		return getValue().toInt32(err);
	}

	if (err) {
		*err = 0;
	}

	if (type == VALUE_TYPE_INT32 || type == VALUE_TYPE_BOOLEAN) {
		return int32Value;
	}

	if (type == VALUE_TYPE_UINT32) {
		return (int32_t)uint32Value;
	}

	if (type == VALUE_TYPE_INT8) {
		return int8Value;
	}
	if (type == VALUE_TYPE_UINT8) {
		return uint8Value;
	}

	if (type == VALUE_TYPE_INT16) {
		return int16Value;
	}
	if (type == VALUE_TYPE_UINT16) {
		return uint16Value;
	}

	if (type == VALUE_TYPE_INT64) {
		return (int32_t)int64Value;
	}
	if (type == VALUE_TYPE_UINT64) {
		return (int32_t)uint64Value;
	}

	if (type == VALUE_TYPE_VALUE_PTR) {
		return pValueValue->toInt32(err);
	}

	if (type == VALUE_TYPE_DOUBLE) {
		return (int32_t)doubleValue;
	}

	if (type == VALUE_TYPE_FLOAT) {
		return (int32_t)floatValue;
	}

	if (isString()) {
        const char *pStart = getString();
        char *pEnd;
		int value = strtol(pStart, &pEnd, 10);
        while (isspace(*pEnd)) {
            pEnd++;
        }
        if (*pEnd == '\0') {
            return value;
        }
	}

    if (err) {
        *err = 1;
    }
	return 0;
}

int64_t Value::toInt64(int *err) const {
	if (isIndirectValueType()) {
		return getValue().toInt64(err);
	}

	if (err) {
		*err = 0;
	}

	if (type == VALUE_TYPE_DOUBLE) {
		return (int64_t)doubleValue;
	}
	if (type == VALUE_TYPE_FLOAT) {
		return (int64_t)floatValue;
	}

	if (type == VALUE_TYPE_INT8) {
		return int8Value;
	}
	if (type == VALUE_TYPE_UINT8) {
		return uint8Value;
	}

	if (type == VALUE_TYPE_INT16) {
		return int16Value;
	}
	if (type == VALUE_TYPE_UINT16) {
		return uint16Value;
	}

	if (type == VALUE_TYPE_INT32 || type == VALUE_TYPE_BOOLEAN) {
		return int32Value;
	}
	if (type == VALUE_TYPE_UINT32) {
		return uint32Value;
	}

	if (type == VALUE_TYPE_INT64) {
		return int64Value;
	}
	if (type == VALUE_TYPE_UINT64) {
		return (int64_t)uint64Value;
	}

	if (isString()) {
        const char *pStart = getString();
        char *pEnd;
		int64_t value = strtol(pStart, &pEnd, 10);
        while (isspace(*pEnd)) {
            pEnd++;
        }
        if (*pEnd == '\0') {
            return value;
        }
	}

    if (err) {
        *err = 1;
    }
	return 0;
}

bool Value::toBool(int *err) const {
	if (isIndirectValueType()) {
		return getValue().toBool(err);
	}

    if (err) {
		*err = 0;
	}

	if (type == VALUE_TYPE_UNDEFINED || type == VALUE_TYPE_NULL) {
		return false;
	}

	if (type == VALUE_TYPE_DOUBLE) {
		return doubleValue != 0;
	}
	if (type == VALUE_TYPE_FLOAT) {
		return floatValue != 0;
	}

	if (type == VALUE_TYPE_INT8) {
		return int8Value != 0;
	}
	if (type == VALUE_TYPE_UINT8) {
		return uint8Value != 0;
	}

	if (type == VALUE_TYPE_INT16) {
		return int16Value != 0;
	}
	if (type == VALUE_TYPE_UINT16) {
		return uint16Value != 0;
	}

	if (type == VALUE_TYPE_INT32 || type == VALUE_TYPE_BOOLEAN) {
		return int32Value != 0;
	}
	if (type == VALUE_TYPE_UINT32) {
		return uint32Value != 0;
	}

	if (type == VALUE_TYPE_INT64) {
		return int64Value != 0;
	}
	if (type == VALUE_TYPE_UINT64) {
		return uint64Value != 0;
	}

	if (type == VALUE_TYPE_DATE) {
		return doubleValue != 0;
	}

	if (isString()) {
		const char *str = getString();
		return str && *str;
	}

	if (isBlob()) {
		auto blobRef = getBlob();
		return blobRef->len > 0;
	}

	if (isArray()) {
		auto array = getArray();
        return array->arraySize != 0;
	}

	if (isDynamicArray()) {
		return getDynamicArray()->arraySize != 0;
	}

    if (isJson()) {
        return int32Value != 0;
    }

    if (isWidget()) {
        return pVoidValue != 0;
    }


    if (err) {
        *err = 1;
    }

	return false;
}

Value Value::toString(uint32_t id) const {
	if (isIndirectValueType()) {
		return getValue().toString(id);
	}

	if (isString()) {
		return *this;
	}

    char tempStr[64];

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4474)
#endif

    if (type == VALUE_TYPE_DOUBLE) {
        snprintf(tempStr, sizeof(tempStr), "%g", doubleValue);
    } else if (type == VALUE_TYPE_FLOAT) {
        snprintf(tempStr, sizeof(tempStr), "%g", floatValue);
    } else if (type == VALUE_TYPE_INT8) {
        snprintf(tempStr, sizeof(tempStr), "%" PRId8 "", int8Value);
    } else if (type == VALUE_TYPE_UINT8) {
        snprintf(tempStr, sizeof(tempStr), "%" PRIu8 "", uint8Value);
    } else if (type == VALUE_TYPE_INT16) {
        snprintf(tempStr, sizeof(tempStr), "%" PRId16 "", int16Value);
    } else if (type == VALUE_TYPE_UINT16) {
        snprintf(tempStr, sizeof(tempStr), "%" PRIu16 "", uint16Value);
    } else if (type == VALUE_TYPE_INT32) {
        snprintf(tempStr, sizeof(tempStr), "%" PRId32 "", int32Value);
    } else if (type == VALUE_TYPE_UINT32) {
        snprintf(tempStr, sizeof(tempStr), "%" PRIu32 "", uint32Value);
    } else if (type == VALUE_TYPE_INT64) {
#ifdef PRId64
        snprintf(tempStr, sizeof(tempStr), "%" PRId64 "", int64Value);
#else
        snprintf(tempStr, sizeof(tempStr), "%" PRId32 "", (int32_t)int64Value);
#endif
    } else if (type == VALUE_TYPE_UINT64) {
#ifdef PRIu64
        snprintf(tempStr, sizeof(tempStr), "%" PRIu64 "", uint64Value);
#else
        snprintf(tempStr, sizeof(tempStr), "%" PRIu32 "", (uint32_t)uint64Value);
#endif
    } else {
        toText(tempStr, sizeof(tempStr));
    }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

	return makeStringRef(tempStr, (int)strlen(tempStr), id);
}

Value Value::makeStringRef(const char *str, int len, uint32_t id) {
	if (len == -1) {
		len = (int)strlen(str);
	}

    char *chars;
    Value value = makeStringRefBuffer(len, id, &chars);
    if (chars == nullptr) {
        return value;
    }

    stringCopyLength(chars, len + 1, str, len);
	chars[len] = 0;

	return value;
}

Value Value::makeStringRefBuffer(int len, uint32_t id, char **str) {
    *str = nullptr;

    // a negative length, or one whose allocation size doesn't fit in size_t,
    // fails like an allocation
    if (len < 0 || (size_t)len > SIZE_MAX - sizeof(StringRef) - 1) {
        return Value(0, VALUE_TYPE_NULL);
    }

    // the StringRef and the characters in one allocation
    auto ptr = alloc(sizeof(StringRef) + len + 1, id);
    if (ptr == nullptr) {
        return Value(0, VALUE_TYPE_NULL);
    }

    StringRef *stringRef = new (ptr) StringRef;
    stringRef->str = (char *)(stringRef + 1);
    stringRef->str[0] = 0;
    stringRef->refCounter = 1;

    *str = stringRef->str;

    Value value;

    value.type = VALUE_TYPE_STRING_REF;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = stringRef;

	return value;
}

Value Value::concatenateString(const Value &str1, const Value &str2) {
    auto s1 = str1.getString();
    auto s2 = str2.getString();
    if (s1 == nullptr || s2 == nullptr) {
        // not a string, e.g. the NULL value of a toString that failed
        return Value(0, VALUE_TYPE_NULL);
    }

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    if (len1 + len2 > INT_MAX) {
        return Value(0, VALUE_TYPE_NULL);
    }

    char *chars;
    Value value = makeStringRefBuffer((int)(len1 + len2), 0xbab14c6a, &chars);
    if (chars == nullptr) {
        return value;
    }

    memcpy(chars, s1, len1);
    memcpy(chars + len1, s2, len2);
    chars[len1 + len2] = 0;

	return value;
}

Value Value::makeArrayRef(int arraySize, int arrayType, uint32_t id) {
    // a negative size, or a size whose byte count doesn't fit in size_t, fails
    // like an allocation
    if (arraySize < 0 || (size_t)arraySize > (SIZE_MAX - sizeof(ArrayValueRef)) / sizeof(Value)) {
        return Value(0, VALUE_TYPE_NULL);
    }

    auto ptr = alloc(sizeof(ArrayValueRef) + (arraySize > 0 ? arraySize - 1 : 0) * sizeof(Value), id);
	if (ptr == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

    ArrayValueRef *arrayRef = new (ptr) ArrayValueRef;
    arrayRef->arrayValue.arraySize = arraySize;
    arrayRef->arrayValue.arrayType = arrayType;
    for (int i = 1; i < arraySize; i++) {
        new (arrayRef->arrayValue.values + i) Value();
    }

    arrayRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_ARRAY_REF;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = arrayRef;

	return value;
}

Value Value::makeArrayElementRef(Value arrayValue, int elementIndex, uint32_t id) {
    auto arrayElementValueRef = ObjectAllocator<ArrayElementValue>::allocate(id);
	if (arrayElementValueRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

    arrayElementValueRef->arrayValue = arrayValue;
    arrayElementValueRef->elementIndex = elementIndex;

    arrayElementValueRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_ARRAY_ELEMENT_VALUE;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = arrayElementValueRef;

	return value;
}

Value Value::makeJsonMemberRef(Value jsonValue, Value propertyName, uint32_t id) {
    auto jsonMemberValueRef = ObjectAllocator<JsonMemberValue>::allocate(id);
	if (jsonMemberValueRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

    jsonMemberValueRef->jsonValue = jsonValue;
    jsonMemberValueRef->propertyName = propertyName;

    jsonMemberValueRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_JSON_MEMBER_VALUE;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = jsonMemberValueRef;

	return value;
}

Value Value::makeBlobRef(const uint8_t *blob, uint32_t len, uint32_t id) {
    auto blobRef = ObjectAllocator<BlobRef>::allocate(id);
	if (blobRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

    // at least one byte, so an empty blob is not taken as a failed allocation
	blobRef->blob = (uint8_t *)alloc(len > 0 ? len : 1, id + 1);
    if (blobRef->blob == nullptr) {
        ObjectAllocator<BlobRef>::deallocate(blobRef);
        return Value(0, VALUE_TYPE_NULL);
    }
    blobRef->len = len;

    if (blob) {
        memcpy(blobRef->blob, blob, len);
    } else {
        memset(blobRef->blob, 0, len);
    }

    blobRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_BLOB_REF;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = blobRef;

	return value;
}

Value Value::makeBlobRef(const uint8_t *blob1, uint32_t len1, const uint8_t *blob2, uint32_t len2, uint32_t id) {
    if (len2 > UINT32_MAX - len1) {
        // the size doesn't fit in the blob length
        return Value(0, VALUE_TYPE_NULL);
    }

    auto blobRef = ObjectAllocator<BlobRef>::allocate(id);
	if (blobRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

    // at least one byte, so an empty blob is not taken as a failed allocation
	blobRef->blob = (uint8_t *)alloc(len1 + len2 > 0 ? len1 + len2 : 1, id + 1);
    if (blobRef->blob == nullptr) {
        ObjectAllocator<BlobRef>::deallocate(blobRef);
        return Value(0, VALUE_TYPE_NULL);
    }
    blobRef->len = len1 + len2;

    memcpy(blobRef->blob, blob1, len1);
    memcpy(blobRef->blob + len1, blob2, len2);

    blobRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_BLOB_REF;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = blobRef;

	return value;
}

#if defined(EEZ_FOR_LVGL)
Value Value::makeLVGLEventRef(uint32_t code, void *currentTarget, void *target, int32_t userData, uint32_t key, int32_t gestureDir, int32_t rotaryDiff, uint32_t id) {
    auto lvglEventRef = ObjectAllocator<LVGLEventRef>::allocate(id);
	if (lvglEventRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

	lvglEventRef->code = code;
    lvglEventRef->currentTarget = currentTarget;
    lvglEventRef->target = target;
    lvglEventRef->userData = userData;
    lvglEventRef->key = key;
    lvglEventRef->gestureDir = gestureDir;
    lvglEventRef->rotaryDiff = rotaryDiff;

    lvglEventRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_EVENT;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = lvglEventRef;

	return value;
}
#endif

Value Value::makePropertyRef(flow::FlowState *flowState, int componentIndex, int propertyIndex, uint32_t id) {
    auto propertyRef = ObjectAllocator<PropertyRef>::allocate(id);
	if (propertyRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

	propertyRef->flowState = flowState;
    propertyRef->componentIndex = componentIndex;
    propertyRef->propertyIndex = propertyIndex;

    propertyRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_PROPERTY_REF;
    value.options = VALUE_OPTIONS_REF;
    value.refValue = propertyRef;

	return value;
}

Value Value::evalProperty() const {
    auto propertyRef = getPropertyRef();
    Value value;
    flow::evalProperty(propertyRef->flowState, propertyRef->componentIndex, propertyRef->propertyIndex, value, flow::FlowError::Plain("Failed to evaluate an user property in UserWidget"));
    return value;
}

Value Value::clone() {
    if (isArray()) {
        auto array = getArray();
        auto resultArrayValue = makeArrayRef(array->arraySize, array->arrayType, 0x0ea48dcb);
        auto resultArray = resultArrayValue.getArray();
        if (resultArray == nullptr) {
            // out of memory: the NULL value
            return resultArrayValue;
        }

        for (uint32_t elementIndex = 0; elementIndex < array->arraySize; elementIndex++) {
            auto elementValue = array->values[elementIndex].clone();
            if (elementValue.type == VALUE_TYPE_NULL && array->values[elementIndex].type != VALUE_TYPE_NULL) {
                // the clone of a string or an array element failed (out of
                // memory): the partial copy is freed
                return elementValue;
            }
            resultArray->values[elementIndex] = elementValue;
        }

        return resultArrayValue;
    } else if (isString()) {
        return makeStringRef(getString(), -1, 0x91846ff3);
    }

    return *this;
}

#if defined(EEZ_OPTION_GUI)

#if !EEZ_OPTION_GUI

Value getVar(int16_t id) {
    auto native_var = native_vars[id];

    if (native_var.type == NATIVE_VAR_TYPE_INTEGER) {
        auto get = (int32_t (*)())native_var.get;
        return Value((int)get(), VALUE_TYPE_INT32);
    }

    if (native_var.type == NATIVE_VAR_TYPE_BOOLEAN) {
        auto get = (bool (*)())native_var.get;
        return Value(get(), VALUE_TYPE_BOOLEAN);
    }

    if (native_var.type == NATIVE_VAR_TYPE_FLOAT) {
        auto get = (float (*)())native_var.get;
        return Value(get(), VALUE_TYPE_FLOAT);
    }

    if (native_var.type == NATIVE_VAR_TYPE_DOUBLE) {
        auto get = (double (*)())native_var.get;
        return Value(get(), VALUE_TYPE_DOUBLE);
    }

    if (native_var.type == NATIVE_VAR_TYPE_STRING) {
        auto get = (const char *(*)())native_var.get;
        return Value(get(), VALUE_TYPE_STRING);
    }

    return Value();
}

void setVar(int16_t id, const Value& value) {
    auto native_var = native_vars[id];

    if (native_var.type == NATIVE_VAR_TYPE_INTEGER) {
        auto set = (void (*)(int32_t))native_var.set;
        set(value.toInt32(nullptr));
    }

    if (native_var.type == NATIVE_VAR_TYPE_BOOLEAN) {
        auto set = (void (*)(bool))native_var.set;
        set(value.toBool(nullptr));
    }

    if (native_var.type == NATIVE_VAR_TYPE_FLOAT) {
        auto set = (void (*)(float))native_var.set;
        set(value.toFloat(nullptr));
    }

    if (native_var.type == NATIVE_VAR_TYPE_DOUBLE) {
        auto set = (void (*)(double))native_var.set;
        set(value.toDouble(nullptr));
    }

    if (native_var.type == NATIVE_VAR_TYPE_STRING) {
        auto set = (void (*)(const char *))native_var.set;
        set(value.getString());
    }
}

#endif // !EEZ_OPTION_GUI

#endif // defined(EEZ_OPTION_GUI)

////////////////////////////////////////////////////////////////////////////////

static const int MAX_FIXED_DECIMALS = 7;

int getPrecisionDecimals(double precision) {
    if (!(precision > 0)) {
        return 0;
    }
    double scaled = precision;
    for (int decimals = 0; decimals < MAX_FIXED_DECIMALS; decimals++) {
        // a whole number, not 0 (with the error of a float precision)
        double whole = round(scaled);
        if (whole != 0 && fabs(scaled - whole) <= 1e-4 * scaled) {
            return decimals;
        }
        scaled *= 10;
    }
    return MAX_FIXED_DECIMALS;
}

static bool hasUnit(Unit unit) {
    return unit != UNIT_NONE && unit != UNIT_UNKNOWN;
}

Value getValueInUnit(const Value &value, Unit unit, double precision) {
    if (!value.isFloat() && !value.isDouble()) {
        return value;
    }

    Unit valueUnit = value.getUnit();
    if (!hasUnit(unit) || (hasUnit(valueUnit) && getBaseUnit(valueUnit) != getBaseUnit(unit))) {
        unit = valueUnit;
    }

    // in the unit
    double number = value.toDouble();
    double factor = 1.0;
    if (hasUnit(valueUnit) && hasUnit(unit) && valueUnit != unit) {
        factor = (double)getUnitFactor(valueUnit) / (double)getUnitFactor(unit);
        number *= factor;
    }

    int decimals;
    if (precision > 0) {
        decimals = getPrecisionDecimals(precision);
    } else if (value.getOptions() & FLOAT_OPTIONS_FIXED_DECIMALS) {
        // the decimals of the value, in the unit: 4 decimals in A are 1 in mA
        // (the number is 1000 times larger)
        decimals = FLOAT_OPTIONS_GET_NUM_FIXED_DECIMALS(value.getOptions()) - (int)round(log10(factor));
        if (decimals < 0) {
            decimals = 0;
        } else if (decimals > MAX_FIXED_DECIMALS) {
            decimals = MAX_FIXED_DECIMALS;
        }
    } else {
        return value;
    }

    Value result;
    if (value.isDouble()) {
        result = Value(number, VALUE_TYPE_DOUBLE);
        result.unit = (uint8_t)unit;
        result.options = FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(decimals);
    } else {
        result = Value((float)number, unit, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(decimals));
    }
    return result;
}

} // namespace eez
