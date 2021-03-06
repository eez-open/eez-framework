/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <eez/core/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

const char *getWidgetLabel(EnumItem *enumDefinition, uint16_t value) {
    for (int i = 0; enumDefinition[i].menuLabel; i++) {
        if (enumDefinition[i].value == value) {
            if (enumDefinition[i].widgetLabel) {
                return enumDefinition[i].widgetLabel;
            }
            return enumDefinition[i].menuLabel;
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

bool compare_UNDEFINED_value(const Value &a, const Value &b) {
    return true;
}

void UNDEFINED_value_to_text(const Value &value, char *text, int count) {
    *text = 0;
}

const char *UNDEFINED_value_type_name(const Value &value) {
    return "undefined";
}

bool compare_NULL_value(const Value &a, const Value &b) {
    return true;
}

void NULL_value_to_text(const Value &value, char *text, int count) {
    *text = 0;
}

const char *NULL_value_type_name(const Value &value) {
    return "null";
}

bool compare_BOOLEAN_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void BOOLEAN_value_to_text(const Value &value, char *text, int count) {
    if (value.getInt()) {
        stringCopy(text, count, "true");
    } else {
        stringCopy(text, count, "false");
    }
}

const char *BOOLEAN_value_type_name(const Value &value) {
    return "boolean";
}

bool compare_INT8_value(const Value &a, const Value &b) {
    return a.getInt8() == b.getInt8();
}

void INT8_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt(text, count, value.getInt8());
}

const char *INT8_value_type_name(const Value &value) {
    return "int8";
}

bool compare_UINT8_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void UINT8_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt32(text, count, value.getUInt8());
}

const char *UINT8_value_type_name(const Value &value) {
    return "uint8";
}

bool compare_INT16_value(const Value &a, const Value &b) {
    return a.getInt16() == b.getInt16();
}

void INT16_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt(text, count, value.getInt16());
}

const char *INT16_value_type_name(const Value &value) {
    return "int16";
}

bool compare_UINT16_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void UINT16_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt32(text, count, value.getUInt16());
}

const char *UINT16_value_type_name(const Value &value) {
    return "uint16";
}

bool compare_INT32_value(const Value &a, const Value &b) {
    return a.getInt32() == b.getInt32();
}

void INT32_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt(text, count, value.getInt32());
}

const char *INT32_value_type_name(const Value &value) {
    return "int32";
}

bool compare_UINT32_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void UINT32_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt32(text, count, value.getUInt32());
}

const char *UINT32_value_type_name(const Value &value) {
    return "uint32";
}

bool compare_INT64_value(const Value &a, const Value &b) {
    return a.getInt64() == b.getInt64();
}

void INT64_value_to_text(const Value &value, char *text, int count) {
    stringAppendInt64(text, count, value.getInt64());
}

const char *INT64_value_type_name(const Value &value) {
    return "int64";
}

bool compare_UINT64_value(const Value &a, const Value &b) {
    return a.getUInt64() == b.getUInt64();
}

void UINT64_value_to_text(const Value &value, char *text, int count) {
    stringAppendUInt64(text, count, value.getUInt64());
}

const char *UINT64_value_type_name(const Value &value) {
    return "uint64";
}

bool compare_FLOAT_value(const Value &a, const Value &b) {
    return a.getUnit() == b.getUnit() && a.getFloat() == b.getFloat() && a.getOptions() == b.getOptions();
}

void FLOAT_value_to_text(const Value &value, char *text, int count) {
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
        if (!fixedDecimals) {
            unit = findDerivedUnit(floatValue, unit);
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

            int n = strlen(text);

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

        stringAppendString(text, count, " ");
        stringAppendString(text, count, getUnitName(unit));
    } else {
        text[0] = 0;
    }
}

const char *FLOAT_value_type_name(const Value &value) {
    return "float";
}

bool compare_DOUBLE_value(const Value &a, const Value &b) {
    return a.getUnit() == b.getUnit() && a.getDouble() == b.getDouble() && a.getOptions() == b.getOptions();
}

void DOUBLE_value_to_text(const Value &value, char *text, int count) {
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
        if (!fixedDecimals) {
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

            int n = strlen(text);

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

        stringAppendString(text, count, " ");
        stringAppendString(text, count, getUnitName(unit));
    } else {
        text[0] = 0;
    }
}

const char *DOUBLE_value_type_name(const Value &value) {
    return "double";
}

bool compare_STRING_value(const Value &a, const Value &b) {
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

void STRING_value_to_text(const Value &value, char *text, int count) {
    const char *str = value.getString();
    if (str) {
        stringCopy(text, count, str);
    } else {
        text[0] = 0;
    }
}

const char *STRING_value_type_name(const Value &value) {
    return "string";
}

bool compare_ARRAY_value(const Value &a, const Value &b) {
    return a.arrayValue == b.arrayValue;
}

void ARRAY_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *ARRAY_value_type_name(const Value &value) {
    return "array";
}

bool compare_ARRAY_REF_value(const Value &a, const Value &b) {
    return a.refValue == b.refValue;
}

void ARRAY_REF_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *ARRAY_REF_value_type_name(const Value &value) {
    return "array";
}

bool compare_STRING_REF_value(const Value &a, const Value &b) {
	return compare_STRING_value(a, b);
}

void STRING_REF_value_to_text(const Value &value, char *text, int count) {
	STRING_value_to_text(value, text, count);
}

const char *STRING_REF_value_type_name(const Value &value) {
    return "string";
}

bool compare_BLOB_REF_value(const Value &a, const Value &b) {
    return a.refValue == b.refValue;
}

void BLOB_REF_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *BLOB_REF_value_type_name(const Value &value) {
    return "blob";
}

bool compare_STREAM_value(const Value &a, const Value &b) {
    return a.int32Value == b.int32Value;
}

void STREAM_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *STREAM_value_type_name(const Value &value) {
    return "stream";
}

bool compare_DATE_value(const Value &a, const Value &b) {
    return a.doubleValue == b.doubleValue;
}

void DATE_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *DATE_value_type_name(const Value &value) {
    return "date";
}

bool compare_VERSIONED_STRING_value(const Value &a, const Value &b) {
    return a.unit == b.unit; // here unit is used as string version
}

void VERSIONED_STRING_value_to_text(const Value &value, char *text, int count) {
    const char *str = value.getString();
    if (str) {
        stringCopy(text, count, str);
    } else {
        text[0] = 0;
    }
}

const char *VERSIONED_STRING_value_type_name(const Value &value) {
    return "versioned-string";
}

bool compare_VALUE_PTR_value(const Value &a, const Value &b) {
	return a.pValueValue == b.pValueValue || (a.pValueValue && b.pValueValue && *a.pValueValue == *b.pValueValue);
}

void VALUE_PTR_value_to_text(const Value &value, char *text, int count) {
	if (value.pValueValue) {
		value.pValueValue->toText(text, count);
	} else {
		text[0] = 0;
	}
}

const char *VALUE_PTR_value_type_name(const Value &value) {
	if (value.pValueValue) {
		return g_valueTypeNames[value.pValueValue->type](value.pValueValue);
	} else {
		return "null";
	}
}

bool compare_ARRAY_ELEMENT_VALUE_value(const Value &a, const Value &b) {
	return a.getValue() == b.getValue();
}

void ARRAY_ELEMENT_VALUE_value_to_text(const Value &value, char *text, int count) {
	value.getValue().toText(text, count);
}

const char *ARRAY_ELEMENT_VALUE_value_type_name(const Value &value) {
    auto value2 = value.getValue();
    return g_valueTypeNames[value2.type](value2);
}

bool compare_FLOW_OUTPUT_value(const Value &a, const Value &b) {
	return a.getUInt16() == b.getUInt16();
}

void FLOW_OUTPUT_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *FLOW_OUTPUT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_NATIVE_VARIABLE_value(const Value &a, const Value &b) {
    auto aValue = get(g_widgetCursor, a.getInt());
    auto bValue = get(g_widgetCursor, b.getInt());
	return aValue == bValue;
}

void NATIVE_VARIABLE_value_to_text(const Value &value, char *text, int count) {
    auto aValue = get(g_widgetCursor, value.getInt());
    aValue.toText(text, count);
}

const char *NATIVE_VARIABLE_value_type_name(const Value &value) {
    auto aValue = get(g_widgetCursor, value.getInt());
    return g_valueTypeNames[aValue.type](aValue);
}

bool compare_RANGE_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void RANGE_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *RANGE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_POINTER_value(const Value &a, const Value &b) {
    return a.getVoidPointer() == b.getVoidPointer();
}

void POINTER_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *POINTER_value_type_name(const Value &value) {
    return "internal";
}

bool compare_ENUM_value(const Value &a, const Value &b) {
    return a.getEnum().enumDefinition == b.getEnum().enumDefinition &&
           a.getEnum().enumValue == b.getEnum().enumValue;
}

void ENUM_value_to_text(const Value &value, char *text, int count) {
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

const char *ENUM_value_type_name(const Value &value) {
    return "internal";
}

bool compare_YT_DATA_GET_VALUE_FUNCTION_POINTER_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

bool compare_IP_ADDRESS_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void IP_ADDRESS_value_to_text(const Value &value, char *text, int count) {
    ipAddressToString(value.getUInt32(), text, count);
}

const char *IP_ADDRESS_value_type_name(const Value &value) {
    return "internal";
}

bool compare_TIME_ZONE_value(const Value &a, const Value &b) {
    return a.getInt16() == b.getInt16();
}

void TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    formatTimeZone(value.getInt16(), text, count);
}

const char *TIME_ZONE_value_type_name(const Value &value) {
    return "internal";
}

void YT_DATA_GET_VALUE_FUNCTION_POINTER_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

const char *YT_DATA_GET_VALUE_FUNCTION_POINTER_value_type_name(const Value &value) {
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
    g_hooks.onArrayValueFreeHook(&arrayValue);
    for (uint32_t i = 1; i < arrayValue.arraySize; i++) {
        (arrayValue.values + i)->~Value();
    }
}

////////////////////////////////////////////////////////////////////////////////

bool assignValue(Value &dstValue, const Value &srcValue) {
    if (dstValue.isBoolean()) {
        dstValue = srcValue;
    } else if (dstValue.isInt32OrLess()) {
        dstValue = srcValue.toInt32();
    } else if (dstValue.isFloat()) {
        dstValue.floatValue = srcValue.toFloat();
    } else if (dstValue.isDouble()) {
        dstValue.doubleValue = srcValue.toDouble();
    } else if (dstValue.isAnyStringType()) {
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

const char *Value::getString() const {
    auto value = getValue();
	if (value.type == VALUE_TYPE_STRING_REF) {
		return ((StringRef *)value.refValue)->str;
	}
	if (value.type == VALUE_TYPE_STRING) {
		return value.strValue;
	}
	return nullptr;
}

const ArrayValue *Value::getArray() const {
    if (type == VALUE_TYPE_ARRAY) {
        return arrayValue;
    }
    return &((ArrayValueRef *)refValue)->arrayValue;
}

ArrayValue *Value::getArray() {
    if (type == VALUE_TYPE_ARRAY) {
        return arrayValue;
    }
    return &((ArrayValueRef *)refValue)->arrayValue;
}

double Value::toDouble(int *err) const {
	if (type == VALUE_TYPE_VALUE_PTR) {
		return pValueValue->toDouble(err);
	}

    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
		return get(g_widgetCursor, getInt()).toDouble(err);
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

	if (type == VALUE_TYPE_STRING) {
		return (double)atof(strValue);
	}

	if (type == VALUE_TYPE_STRING_REF) {
		return (double)atof(((StringRef *)refValue)->str);
	}

    if (err) {
        *err = 1;
    }
	return NAN;
}

float Value::toFloat(int *err) const {
	if (type == VALUE_TYPE_VALUE_PTR) {
		return pValueValue->toFloat(err);
	}

    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
		return get(g_widgetCursor, getInt()).toFloat(err);
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

	if (type == VALUE_TYPE_STRING) {
		return (float)atof(strValue);
	}

	if (type == VALUE_TYPE_STRING_REF) {
		return (float)atof(((StringRef *)refValue)->str);
	}

    if (err) {
        *err = 1;
    }
	return NAN;
}

int32_t Value::toInt32(int *err) const {
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

    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
		return get(g_widgetCursor, getInt()).toInt32(err);
	}

	if (type == VALUE_TYPE_DOUBLE) {
		return (int32_t)doubleValue;
	}

	if (type == VALUE_TYPE_FLOAT) {
		return (int32_t)floatValue;
	}

	if (type == VALUE_TYPE_STRING) {
		return (int64_t)atoi(strValue);
	}

	if (type == VALUE_TYPE_STRING_REF) {
		return (int64_t)atoi(((StringRef *)refValue)->str);
	}

    if (err) {
        *err = 1;
    }
	return 0;
}

int64_t Value::toInt64(int *err) const {
	if (type == VALUE_TYPE_VALUE_PTR) {
		return pValueValue->toInt64(err);
	}

    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
		return get(g_widgetCursor, getInt()).toInt64(err);
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

	if (type == VALUE_TYPE_STRING) {
		return (int64_t)atoi(strValue);
	}

	if (type == VALUE_TYPE_STRING_REF) {
		return (int64_t)atoi(((StringRef *)refValue)->str);
	}

    if (err) {
        *err = 1;
    }
	return 0;
}

bool Value::toBool(int *err) const {
	if (type == VALUE_TYPE_VALUE_PTR) {
		return pValueValue->toBool(err);
	}

    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
		return get(g_widgetCursor, getInt()).toBool(err);
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

	if (type == VALUE_TYPE_STRING) {
		return strValue && *strValue;
	}

	if (type == VALUE_TYPE_STRING_REF) {
        auto strValue = toString(0xdbb61edf);
		const char *str = strValue.getString();
		return str && *str;
	}

	if (type == VALUE_TYPE_ARRAY || type == VALUE_TYPE_ARRAY_REF) {
		auto arrayValue = getArray();
        return arrayValue->arraySize != 0;
	}

    if (err) {
        *err = 1;
    }

	return false;
}

Value Value::toString(uint32_t id) const {
	if (type == VALUE_TYPE_VALUE_PTR) {
		return pValueValue->toString(id);
	}

    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
		return get(g_widgetCursor, getInt()).toString(id);
	}

	if (type == VALUE_TYPE_STRING || type == VALUE_TYPE_STRING_REF) {
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
        snprintf(tempStr, sizeof(tempStr), "%" PRId64 "", int64Value);
    } else if (type == VALUE_TYPE_UINT64) {
        snprintf(tempStr, sizeof(tempStr), "%" PRIu64 "", uint64Value);
    } else {
        toText(tempStr, sizeof(tempStr));
    }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

	return makeStringRef(tempStr, strlen(tempStr), id);
}

Value Value::makeStringRef(const char *str, int len, uint32_t id) {
    auto stringRef = ObjectAllocator<StringRef>::allocate(id);
	if (stringRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

	if (len == -1) {
		len = strlen(str);
	}

    stringRef->str = (char *)alloc(len + 1, id + 1);
    if (stringRef->str == nullptr) {
        ObjectAllocator<StringRef>::deallocate(stringRef);
        return Value(0, VALUE_TYPE_NULL);
    }

    stringCopyLength(stringRef->str, len + 1, str, len);
	stringRef->str[len] = 0;

    stringRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_STRING_REF;
    value.unit = 0;
    value.options = VALUE_OPTIONS_REF;
    value.reserved = 0;
    value.refValue = stringRef;

	return value;
}

Value Value::concatenateString(const Value &str1, const Value &str2) {
    auto stringRef = ObjectAllocator<StringRef>::allocate(0xbab14c6a);;
	if (stringRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

    auto newStrLen = strlen(str1.getString()) + strlen(str2.getString()) + 1;
    stringRef->str = (char *)alloc(newStrLen, 0xb5320162);
    if (stringRef->str == nullptr) {
        ObjectAllocator<StringRef>::deallocate(stringRef);
        return Value(0, VALUE_TYPE_NULL);
    }

    stringCopy(stringRef->str, newStrLen, str1.getString());
    stringAppendString(stringRef->str, newStrLen, str2.getString());

    stringRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_STRING_REF;
    value.unit = 0;
    value.options = VALUE_OPTIONS_REF;
    value.reserved = 0;
    value.refValue = stringRef;

	return value;
}

Value Value::makeArrayRef(int arraySize, int arrayType, uint32_t id) {
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
    value.unit = 0;
    value.options = VALUE_OPTIONS_REF;
    value.reserved = 0;
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
    value.unit = 0;
    value.options = VALUE_OPTIONS_REF;
    value.reserved = 0;
    value.refValue = arrayElementValueRef;

	return value;
}

Value Value::makeBlobRef(const uint8_t *blob, uint32_t len, uint32_t id) {
    auto blobRef = ObjectAllocator<BlobRef>::allocate(id);
	if (blobRef == nullptr) {
		return Value(0, VALUE_TYPE_NULL);
	}

	blobRef->blob = (uint8_t *)alloc(len, id + 1);
    if (blobRef->blob == nullptr) {
        ObjectAllocator<BlobRef>::deallocate(blobRef);
        return Value(0, VALUE_TYPE_NULL);
    }
    blobRef->len = len;

    memcpy(blobRef->blob, blob, len);

    blobRef->refCounter = 1;

    Value value;

    value.type = VALUE_TYPE_BLOB_REF;
    value.unit = 0;
    value.options = VALUE_OPTIONS_REF;
    value.reserved = 0;
    value.refValue = blobRef;

	return value;
}

////////////////////////////////////////////////////////////////////////////////

void data_none(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    value = Value();
}

////////////////////////////////////////////////////////////////////////////////

int count(const WidgetCursor &widgetCursor, int16_t id) {
    Value countValue = 0;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_COUNT, widgetCursor, countValue);
    return countValue.getInt();
}

void select(WidgetCursor &widgetCursor, int16_t id, int index, Value &oldValue) {
    Value cursorValue = index;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CURSOR_VALUE, widgetCursor, cursorValue);

	widgetCursor.cursor = cursorValue.getInt();

    Value indexValue = index;

    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SELECT, widgetCursor, indexValue);

    if (index == 0) {
        oldValue = indexValue;
    }
}

void deselect(WidgetCursor &widgetCursor, int16_t id, Value &oldValue) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_DESELECT, widgetCursor, oldValue);
}

void setContext(WidgetCursor &widgetCursor, int16_t id, Value &oldContext, Value &newContext) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SET_CONTEXT, widgetCursor, oldContext);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CONTEXT, widgetCursor, newContext);

    Value cursorValue;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CONTEXT_CURSOR, widgetCursor, cursorValue);
    widgetCursor.cursor = cursorValue.getInt();
}

void restoreContext(WidgetCursor &widgetCursor, int16_t id, Value &oldContext) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_RESTORE_CONTEXT, widgetCursor, oldContext);
}

int getFloatListLength(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_FLOAT_LIST_LENGTH, widgetCursor, value);
    return value.getInt();
}

float *getFloatList(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_FLOAT_LIST, widgetCursor, value);
    return value.getFloatList();
}

bool getAllowZero(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ALLOW_ZERO, widgetCursor, value);
    return value.getInt() != 0;
}

Value getMin(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_MIN, widgetCursor, value);
    return value;
}

Value getMax(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_MAX, widgetCursor, value);
    return value;
}

Value getDef(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_DEF, widgetCursor, value);
    return value;
}

Value getPrecision(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_PRECISION, widgetCursor, value);
    return value;
}

Value getLimit(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_LIMIT, widgetCursor, value);
    return value;
}

const char *getName(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_NAME, widgetCursor, value);
    return value.getString();
}

Unit getUnit(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_UNIT, widgetCursor, value);
    return (Unit)value.getInt();
}

bool isChannelData(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_IS_CHANNEL_DATA, widgetCursor, value);
    return value.getInt() != 0;
}

void getLabel(const WidgetCursor &widgetCursor, int16_t id, char *text, int count) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_LABEL, widgetCursor, value);
    value.toText(text, count);
}

void getEncoderStepValues(const WidgetCursor &widgetCursor, int16_t id, StepValues &stepValues) {
    Value value(&stepValues, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ENCODER_STEP_VALUES, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_INT32 && value.getInt()) {
        if (stepValues.encoderSettings.mode != ENCODER_MODE_AUTO) {
            stepValues.encoderSettings.accelerationEnabled = false;
        }
    } else {
        stepValues.count = 0;
        stepValues.encoderSettings.accelerationEnabled = true;
        stepValues.encoderSettings.mode = ENCODER_MODE_AUTO;
        stepValues.encoderSettings.range = getMax(widgetCursor, id).toFloat() - getMin(widgetCursor, id).toFloat();
        Value precisionValue = getPrecision(widgetCursor, id);
        stepValues.encoderSettings.step = precisionValue.toFloat();
    }
}

void setEncoderMode(const WidgetCursor &widgetCursor, int16_t id, EncoderMode encoderMode) {
	Value value(encoderMode, VALUE_TYPE_INT32);
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SET_ENCODER_MODE, widgetCursor, value);
}

Value get(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET, widgetCursor, value);
    return value;
}

const char *isValidValue(const WidgetCursor &widgetCursor, int16_t id, Value value) {
    Value savedValue = value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_VALID_VALUE, widgetCursor, value);
    return value != savedValue ? value.getString() : nullptr;
}

Value set(const WidgetCursor &widgetCursor, int16_t id, Value value) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SET, widgetCursor, value);
    return value;
}

Value getDisplayValueRange(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_DISPLAY_VALUE_RANGE, widgetCursor, value);
    return value;
}

uint32_t getTextRefreshRate(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_TEXT_REFRESH_RATE, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT32) {
        return value.getUInt32();
    }
    return 0;
}

uint16_t getColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->color;
}

uint16_t getBackgroundColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_BACKGROUND_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->backgroundColor;
}

uint16_t getActiveColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ACTIVE_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->activeColor;
}

uint16_t getActiveBackgroundColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->activeBackgroundColor;
}

bool isBlinking(const WidgetCursor &widgetCursor, int16_t id) {
    if (id == DATA_ID_NONE) {
        return false;
    }

    if (widgetCursor.appContext->isBlinking(widgetCursor, id)) {
        return true;
    }

    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_BLINKING, widgetCursor, value);
    return value.getInt() ? true : false;
}

Value getEditValue(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET, widgetCursor, value);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_EDIT_VALUE, widgetCursor, value);
    return value;
}

Value getBitmapImage(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_BITMAP_IMAGE, widgetCursor, value);
    return value;
}

uint32_t ytDataGetRefreshCounter(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER, widgetCursor, value);
    return value.getUInt32();
}

uint32_t ytDataGetSize(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_SIZE, widgetCursor, value);
    return value.getUInt32();

}

uint32_t ytDataGetPosition(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_POSITION, widgetCursor, value);
    return value.getUInt32();
}

void ytDataSetPosition(const WidgetCursor &widgetCursor, int16_t id, uint32_t newPosition) {
	Value value(newPosition, VALUE_TYPE_UINT32);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_SET_POSITION, widgetCursor, value);
}

uint32_t ytDataGetPositionIncrement(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT32) {
        return value.getUInt32();
    }
    return 1;
}

uint32_t ytDataGetPageSize(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_PAGE_SIZE, widgetCursor, value);
    return value.getUInt32();
}

const Style *ytDataGetStyle(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_STYLE, widgetCursor, value);
    return getStyle(value.getUInt16());
}

Value ytDataGetMin(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_MIN, widgetCursor, value);
    return value;
}

Value ytDataGetMax(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_MAX, widgetCursor, value);
    return value;
}

int ytDataGetVertDivisions(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS, widgetCursor, value);
    return value.getInt();
}

int ytDataGetHorzDivisions(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS, widgetCursor, value);
    return value.getInt();
}

float ytDataGetDiv(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_DIV, widgetCursor, value);
    return value.getFloat();
}

float ytDataGetOffset(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_OFFSET, widgetCursor, value);
    return value.getFloat();
}

bool ytDataDataValueIsVisible(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE), widgetCursor, value);
    return value.getInt();
}

bool ytDataGetShowLabels(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_GET_SHOW_LABELS), widgetCursor, value);
    return value.getInt();
}

int8_t ytDataGetSelectedValueIndex(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX), widgetCursor, value);
    return (int8_t)value.getInt();
}

void ytDataGetLabel(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex, char *text, int count) {
    text[0] = 0;
    YtDataGetLabelParams params = {
        valueIndex,
        text,
        count
    };
    Value value(&params, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_GET_LABEL), widgetCursor, value);
}

Value::YtDataGetValueFunctionPointer ytDataGetGetValueFunc(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC, widgetCursor, value);
    return value.getYtDataGetValueFunctionPointer();
}

uint8_t ytDataGetGraphUpdateMethod(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD, widgetCursor, value);
    return value.getUInt8();
}

float ytDataGetPeriod(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_PERIOD, widgetCursor, value);
    return value.getFloat();
}

uint8_t *ytDataGetBookmarks(const WidgetCursor &widgetCursor, int16_t id) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_BOOKMARKS, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_POINTER) {
        return (uint8_t *)value.getVoidPointer();
    }
    return nullptr;
}

bool ytDataIsCursorVisible(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE, widgetCursor, value);
    return value.getInt() == 1;
}

uint32_t ytDataGetCursorOffset(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET, widgetCursor, value);
    return value.getUInt32();
}

Value ytDataGetCursorXValue(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE, widgetCursor, value);
    return value;
}

void ytDataTouchDrag(const WidgetCursor &widgetCursor, int16_t id, TouchDrag *touchDrag) {
    Value value = Value(touchDrag, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_TOUCH_DRAG, widgetCursor, value);
}

int getTextCursorPosition(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_TEXT_CURSOR_POSITION, widgetCursor, value);
    return value.getType() == VALUE_TYPE_INT32 ? value.getInt() : -1;
}

int getXScroll(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    Value value((void *)&widgetCursor, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET_X_SCROLL, widgetCursor, value);
    return value.getType() == VALUE_TYPE_INT32 ? value.getInt() : 0;
}

void getSlotAndSubchannelIndex(const WidgetCursor &widgetCursor, int16_t id, int &slotIndex, int &subchannelIndex) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_SLOT_AND_SUBCHANNEL_INDEX, widgetCursor, value);
	if (value.getType() == VALUE_TYPE_UINT32) {
		slotIndex = value.getFirstInt16();
		subchannelIndex = value.getSecondInt16();
	}
}

bool isMicroAmperAllowed(const WidgetCursor &widgetCursor, int16_t id) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_MICRO_AMPER_ALLOWED, widgetCursor, value);
	return value.getInt() == 1;
}

bool isAmperAllowed(const WidgetCursor &widgetCursor, int16_t id) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_AMPER_ALLOWED, widgetCursor, value);
	return value.getInt() == 1;
}

CanvasDrawFunction getCanvasDrawFunction(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_POINTER) {
        return (CanvasDrawFunction)value.getVoidPointer();
    }
    return nullptr;
}

Value getCanvasRefreshState(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CANVAS_REFRESH_STATE, widgetCursor, value);
    return value;
}

} // namespace gui
} // namespace eez
