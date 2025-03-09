/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <string.h>
#include <eez/core/value_types.h>
#include <eez/core/alloc.h>
#include <eez/flow/flow_defs_v3.h>

namespace eez {

namespace flow {
    struct FlowState;
}

static const size_t MAX_ITERATORS = 4;

////////////////////////////////////////////////////////////////////////////////

#if EEZ_OPTION_GUI
namespace gui {
    class AppContext;
}
using gui::AppContext;
#endif

////////////////////////////////////////////////////////////////////////////////

namespace gui {
    struct Style;
}

////////////////////////////////////////////////////////////////////////////////

struct EnumValue {
    uint16_t enumValue;
    uint16_t enumDefinition;
};

////////////////////////////////////////////////////////////////////////////////

#define VALUE_OPTIONS_REF (1 << 0)

#define STRING_OPTIONS_FILE_ELLIPSIS (1 << 1)

#define FLOAT_OPTIONS_LESS_THEN (1 << 1)
#define FLOAT_OPTIONS_FIXED_DECIMALS (1 << 2)
#define FLOAT_OPTIONS_GET_NUM_FIXED_DECIMALS(options) (((options) >> 3) & 0b111)
#define FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(n) (FLOAT_OPTIONS_FIXED_DECIMALS | ((n & 0b111) << 3))

////////////////////////////////////////////////////////////////////////////////

struct Value;

typedef bool (*CompareValueFunction)(const Value &a, const Value &b);
typedef void (*ValueToTextFunction)(const Value &value, char *text, int count);
typedef const char * (*ValueTypeNameFunction)(const Value &value);
typedef void (*CopyValueFunction)(Value &a, const Value &b);

extern CompareValueFunction g_valueTypeCompareFunctions[];
extern ValueToTextFunction g_valueTypeToTextFunctions[];
extern ValueTypeNameFunction g_valueTypeNames[];

////////////////////////////////////////////////////////////////////////////////

struct PairOfUint8Value {
    uint8_t first;
    uint8_t second;
};

struct PairOfUint16Value {
    uint16_t first;
    uint16_t second;
};

struct PairOfInt16Value {
    int16_t first;
    int16_t second;
};

struct Ref {
	uint32_t refCounter;
    virtual ~Ref() {}
};

struct ArrayValue;
struct ArrayElementValue;
struct BlobRef;
struct PropertyRef;

#if defined(EEZ_FOR_LVGL)
struct LVGLEventRef;
#endif

#if defined(EEZ_DASHBOARD_API)
namespace flow {
    extern void dashboardObjectValueIncRef(int json);
    extern void dashboardObjectValueDecRef(int json);
}
#endif

////////////////////////////////////////////////////////////////////////////////

struct Value {
  public:
    Value()
        : type(VALUE_TYPE_UNDEFINED), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), uint64Value(0)
    {
    }

	Value(int value)
        : type(VALUE_TYPE_INT32), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), int32Value(value)
    {
    }

	Value(const char *str)
        : type(VALUE_TYPE_STRING), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), strValue(str)
    {
    }

	Value(uint8_t version, const char *str)
        : type(VALUE_TYPE_VERSIONED_STRING), unit(version), options(0), dstValueType(VALUE_TYPE_UNDEFINED), strValue(str)
    {
    }

	Value(Value *pValue)
		: type(VALUE_TYPE_VALUE_PTR), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), pValueValue(pValue)
    {
        // if (pValue->options & VALUE_OPTIONS_REF) {
		// 	pValue->refValue->refCounter++;
		// }
	}

    Value(const char *str, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), strValue(str)
    {
    }

    Value(int value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), int32Value(value)
    {
    }

    Value(int value, ValueType type_, uint16_t options_)
        : type(type_), unit(UNIT_UNKNOWN), options(options_), dstValueType(VALUE_TYPE_UNDEFINED), int32Value(value)
    {
    }

    Value(int8_t value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), int8Value(value)
    {
    }

    Value(uint8_t value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), uint8Value(value)
    {
    }

    Value(int16_t value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), int16Value(value)
    {
    }

    Value(uint16_t value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), uint16Value(value)
    {
    }

    Value(uint32_t value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), uint32Value(value)
    {
    }

    Value(int64_t value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), int64Value(value)
    {
    }

    Value(uint64_t value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), uint64Value(value)
    {
    }

    Value(float value, Unit unit_)
        : type(VALUE_TYPE_FLOAT), unit(unit_), options(0), dstValueType(VALUE_TYPE_UNDEFINED), floatValue(value)
    {
    }

    Value(float value, Unit unit_, uint16_t options_)
        : type(VALUE_TYPE_FLOAT), unit(unit_), options(options_), dstValueType(VALUE_TYPE_UNDEFINED), floatValue(value)
    {
    }

    Value(float value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), floatValue(value)
    {
    }

	Value(double value, ValueType type_)
		: type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), doubleValue(value) {
	}

	Value(const char *value, ValueType type_, uint16_t options_)
        : type(type_), unit(UNIT_UNKNOWN), options(options_), dstValueType(VALUE_TYPE_UNDEFINED), strValue(value)
    {
    }

    Value(void *value, ValueType type_)
        : type(type_), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), pVoidValue(value)
    {
    }

    typedef float (*YtDataGetValueFunctionPointer)(uint32_t rowIndex, uint8_t columnIndex, float *max);

    Value(YtDataGetValueFunctionPointer ytDataGetValueFunctionPointer)
        : type(VALUE_TYPE_YT_DATA_GET_VALUE_FUNCTION_POINTER), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), pVoidValue((void *)ytDataGetValueFunctionPointer)
    {
    }

	Value(const Value& value)
		: type(VALUE_TYPE_UNDEFINED), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), uint64Value(0)
	{
		*this = value;
	}

#if EEZ_OPTION_GUI
    Value(AppContext *appContext)
        : type(VALUE_TYPE_POINTER), unit(UNIT_UNKNOWN), options(0), dstValueType(VALUE_TYPE_UNDEFINED), pVoidValue(appContext)
    {
    }
#endif

	~Value() {
        freeRef();
	}

    void freeRef() {
		if (options & VALUE_OPTIONS_REF) {
			if (--refValue->refCounter == 0) {
                ObjectAllocator<Ref>::deallocate(refValue);
			}
		}/* else if (type == VALUE_TYPE_VALUE_PTR) {
            if (pValueValue->options & VALUE_OPTIONS_REF) {
                if (--pValueValue->refValue->refCounter == 0) {
                    ObjectAllocator<Ref>::deallocate(pValueValue->refValue);
                }
            }
        }*/

#if defined(EEZ_DASHBOARD_API)
        if (type == VALUE_TYPE_JSON || type == VALUE_TYPE_STREAM) {
            flow::dashboardObjectValueDecRef(int32Value);
        }
#endif
    }

    Value& operator = (const Value &value) {
        freeRef();

        if (value.type == VALUE_TYPE_STRING_ASSET) {
            type = VALUE_TYPE_STRING;
            unit = 0;
            options = 0;

#if __GNUC__ && defined( __has_warning )
#   if __has_warning( "-Wdangling-pointer" )
#       define SUPPRESSING
#       pragma GCC diagnostic push
#       pragma GCC diagnostic ignored "-Wdangling-pointer"
#   endif
#endif

            strValue = (const char *)((uint8_t *)&value.int32Value + value.int32Value);

#ifdef SUPPRESSING
#   undef SUPPRESSING
#   pragma GCC diagnostic pop
#endif

        } else if (value.type == VALUE_TYPE_ARRAY_ASSET) {
            type = VALUE_TYPE_ARRAY;
            unit = 0;
            options = 0;

#if __GNUC__ && defined( __has_warning )
#   if __has_warning( "-Wdangling-pointer" )
#       define SUPPRESSING
#       pragma GCC diagnostic push
#       pragma GCC diagnostic ignored "-Wdangling-pointer"
#   endif
#endif
            arrayValue = (ArrayValue *)((uint8_t *)&value.int32Value + value.int32Value);

#ifdef SUPPRESSING
#   undef SUPPRESSING
#   pragma GCC diagnostic pop
#endif

        } else {
            type = value.type;
            unit = value.unit;
            options = value.options;
            dstValueType = value.dstValueType;
            memcpy((void *)&int64Value, (const void *)&value.int64Value, sizeof(int64_t));

            if (options & VALUE_OPTIONS_REF) {
                refValue->refCounter++;
            } /* else if (type == VALUE_TYPE_VALUE_PTR) {
                if (pValueValue->options & VALUE_OPTIONS_REF) {
                    pValueValue->refValue->refCounter++;
                }
            }*/

#if defined(EEZ_DASHBOARD_API)
            if (type == VALUE_TYPE_JSON || type == VALUE_TYPE_STREAM) {
                flow::dashboardObjectValueIncRef(value.int32Value);;
            }
#endif
        }

        return *this;
    }

    bool operator==(const Value &other) const {
		return g_valueTypeCompareFunctions[type](*this, other);
	}


    bool operator!=(const Value &other) const {
        return !(*this == other);
    }

    ValueType getType() const {
        return (ValueType)type;
    }

    bool isIndirectValueType() const {
        return type == VALUE_TYPE_VALUE_PTR || type == VALUE_TYPE_NATIVE_VARIABLE || type == VALUE_TYPE_ARRAY_ELEMENT_VALUE || type == VALUE_TYPE_JSON_MEMBER_VALUE || type == VALUE_TYPE_PROPERTY_REF;
    }

    Value getValue() const;

    bool isUndefinedOrNull() {
        return type == VALUE_TYPE_UNDEFINED || type == VALUE_TYPE_NULL;
    }

	static bool isInt32OrLess(int type) {
		return (type >= VALUE_TYPE_INT8 && type <= VALUE_TYPE_UINT32) || type == VALUE_TYPE_BOOLEAN;
	}

	bool isInt32OrLess() const {
		return (type >= VALUE_TYPE_INT8 && type <= VALUE_TYPE_UINT32) || type == VALUE_TYPE_BOOLEAN;
	}

	bool isInt64() const {
		return type == VALUE_TYPE_INT64 || type == VALUE_TYPE_UINT64;
	}

	bool isInt32() const {
		return type == VALUE_TYPE_INT32 || type == VALUE_TYPE_UINT32;
	}

	bool isInt16() const {
		return type == VALUE_TYPE_INT16 || type == VALUE_TYPE_UINT16;
	}

	bool isInt8() const {
		return type == VALUE_TYPE_INT8 || type == VALUE_TYPE_UINT8;
	}

	bool isFloat() const {
        return type == VALUE_TYPE_FLOAT;
    }

	bool isDouble() const {
		return type == VALUE_TYPE_DOUBLE || type == VALUE_TYPE_DATE;
	}

	bool isBoolean() const {
		return type == VALUE_TYPE_BOOLEAN;
	}

	bool isString() const {
        return type == VALUE_TYPE_STRING || type == VALUE_TYPE_STRING_ASSET || type == VALUE_TYPE_STRING_REF;
    }

    bool isArray() const {
        return type == VALUE_TYPE_ARRAY || type == VALUE_TYPE_ARRAY_ASSET || type == VALUE_TYPE_ARRAY_REF;
    }

	bool isBlob() const {
        return type == VALUE_TYPE_BLOB_REF;
    }

	bool isJson() const {
        return type == VALUE_TYPE_JSON;
    }

    bool isWidget() const {
        return type == VALUE_TYPE_WIDGET;
    }

    bool isError() const {
        return type == VALUE_TYPE_ERROR;
    }

    Unit getUnit() const {
        return (Unit)unit;
    }

	bool getBoolean() const {
		return int32Value;
	}

	int8_t getInt8() const {
		return int8Value;
	}

	uint8_t getUInt8() const {
        return uint8Value;
    }

	int16_t getInt16() const {
		return int16Value;
	}

	uint16_t getUInt16() const {
        return uint16Value;
    }

	int32_t getInt32() const {
		return int32Value;
	}

	uint32_t getUInt32() const {
        return uint32Value;
    }

	int64_t getInt64() const {
		return int64Value;
	}

	uint64_t getUInt64() const {
        return uint64Value;
    }

	float getFloat() const {
		return floatValue;
	}

	double getDouble() const {
		return doubleValue;
	}

	const char *getString() const;

    const ArrayValue *getArray() const;
    ArrayValue *getArray();

    //////////

	int getInt() const {
		if (type == VALUE_TYPE_ENUM) {
			return enumValue.enumValue;
		}
		return int32Value;
	}

    const EnumValue &getEnum() const {
        return enumValue;
    }

    int16_t getScpiError() const {
        return int16Value;
    }

    uint8_t *getPUint8() const {
        return puint8Value;
    }

    float *getFloatList() const {
        return pFloatValue;
    }

    void *getVoidPointer() const {
        return pVoidValue;
    }

    YtDataGetValueFunctionPointer getYtDataGetValueFunctionPointer() const {
        return (YtDataGetValueFunctionPointer)pVoidValue;
    }

    uint8_t getFirstUInt8() const {
        return pairOfUint8Value.first;
    }
    uint8_t getSecondUInt8() const {
        return pairOfUint8Value.second;
    }

    uint16_t getFirstUInt16() const {
        return pairOfUint16Value.first;
    }
    uint16_t getSecondUInt16() const {
        return pairOfUint16Value.second;
    }

    int16_t getFirstInt16() const {
        return pairOfInt16Value.first;
    }
    int16_t getSecondInt16() const {
        return pairOfInt16Value.second;
    }

    BlobRef *getBlob() const {
        return (BlobRef *)refValue;
    }

    void *getWidget() {
        return pVoidValue;
    }

#if defined(EEZ_FOR_LVGL)
    LVGLEventRef *getLVGLEventRef() const {
        return (LVGLEventRef *)refValue;
    }
#endif

    void toText(char *text, int count) const {
		*text = 0;
		g_valueTypeToTextFunctions[type](*this, text, count);
	}

	uint16_t getOptions() const {
        return options;
    }

    uint16_t getRangeFrom() {
        return pairOfUint16Value.first;
    }

    uint16_t getRangeTo() {
        return pairOfUint16Value.second;
    }

	double toDouble(int *err = nullptr) const;
	float toFloat(int *err = nullptr) const;
	int32_t toInt32(int *err = nullptr) const;
	int64_t toInt64(int *err = nullptr) const;
    bool toBool(int *err = nullptr) const;

	Value toString(uint32_t id) const;

	static Value makeStringRef(const char *str, int len, uint32_t id);
	static Value concatenateString(const Value &str1, const Value &str2);

    static Value makeArrayRef(int arraySize, int arrayType, uint32_t id);
    static Value makeArrayElementRef(Value arrayValue, int elementIndex, uint32_t id);
    static Value makeJsonMemberRef(Value jsonValue, Value propertyName, uint32_t id);

    static Value makeBlobRef(const uint8_t *blob, uint32_t len, uint32_t id);
    static Value makeBlobRef(const uint8_t *blob1, uint32_t len1, const uint8_t *blob2, uint32_t len2, uint32_t id);

#if defined(EEZ_FOR_LVGL)
    static Value makeLVGLEventRef(uint32_t code, void *currentTarget, void *target, int32_t userData, uint32_t key, int32_t gestureDir, int32_t rotaryDiff, uint32_t id);
#endif

    static Value makeError() { return Value(0, VALUE_TYPE_ERROR); }

    static Value makePropertyRef(flow::FlowState *flowState, int componentIndex, int propertyIndex, uint32_t id);
    PropertyRef *getPropertyRef() const {
        return (PropertyRef *)refValue;
    }
    Value evalProperty() const;

    Value clone();

	//////////

  public:
	uint8_t type;
	uint8_t unit;
	uint16_t options;

    uint32_t dstValueType;

    union {
		int8_t int8Value;
		uint8_t uint8Value;
		int16_t int16Value;
		uint16_t uint16Value;
		int32_t int32Value;
		uint32_t uint32Value;
		int64_t int64Value;
		uint64_t uint64Value;

		float floatValue;
		double doubleValue;

		const char *strValue;
		ArrayValue *arrayValue;
		Ref *refValue;

		uint8_t *puint8Value;
		float *pFloatValue;
		void *pVoidValue;
		Value *pValueValue;

		EnumValue enumValue;

		PairOfUint8Value pairOfUint8Value;
		PairOfUint16Value pairOfUint16Value;
		PairOfInt16Value pairOfInt16Value;
	};
};

struct StringRef : public Ref {
    ~StringRef() {
        if (str) {
            eez::free(str);
        }
    }
	char *str;
};

struct ArrayValue {
	uint32_t arraySize;
    uint32_t arrayType;
	Value values[1];
};

struct ArrayValueRef : public Ref {
    ~ArrayValueRef();
	ArrayValue arrayValue;
};

struct BlobRef : public Ref {
    ~BlobRef() {
        if (blob) {
            eez::free(blob);
        }
    }
	uint8_t *blob;
    uint32_t len;
};

#if defined(EEZ_FOR_LVGL)
struct LVGLEventRef : public Ref {
	uint32_t code;
    void *currentTarget;
    void *target;
    int32_t userData;
    uint32_t key;
    int32_t gestureDir;
    int32_t rotaryDiff;
};
#endif

struct PropertyRef : public Ref {
	flow::FlowState *flowState;
    int componentIndex;
    int propertyIndex;
};

struct ArrayElementValue : public Ref {
	Value arrayValue;
    int elementIndex;
    uint32_t dstValueType;
};

struct JsonMemberValue : public Ref {
	Value jsonValue;
    Value propertyName;
};

#if EEZ_OPTION_GUI
namespace gui {
    struct WidgetCursor;
    extern gui::WidgetCursor g_widgetCursor;
    extern Value get(const gui::WidgetCursor &widgetCursor, int16_t id);
}
#else
Value getVar(int16_t id);
void setVar(int16_t id, const Value& value);
#endif

#if defined(EEZ_DASHBOARD_API)
namespace flow {
    extern Value operationJsonGet(int json, const char *property);
    extern Value getObjectVariableMemberValue(Value *objectValue, int memberIndex);
}
#endif

inline Value Value::getValue() const {
    if (type == VALUE_TYPE_VALUE_PTR) {
        return pValueValue->getValue();
    }
    if (type == VALUE_TYPE_NATIVE_VARIABLE) {
#if EEZ_OPTION_GUI
        using namespace gui;
        return get(g_widgetCursor, int32Value);
#else
        return getVar(int32Value);
#endif
    }
    if (type == VALUE_TYPE_ARRAY_ELEMENT_VALUE) {
        auto arrayElementValue = (ArrayElementValue *)refValue;
        if (arrayElementValue->arrayValue.isBlob()) {
            auto blobRef = arrayElementValue->arrayValue.getBlob();
            if (arrayElementValue->elementIndex < 0 || arrayElementValue->elementIndex >= (int)blobRef->len) {
                return Value();
            }
            return Value((uint32_t)blobRef->blob[arrayElementValue->elementIndex], VALUE_TYPE_UINT32);
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

////////////////////////////////////////////////////////////////////////////////

bool assignValue(Value &dstValue, const Value &srcValue, uint32_t dstValueType = VALUE_TYPE_UNDEFINED);

////////////////////////////////////////////////////////////////////////////////

uint16_t getPageIndexFromValue(const Value &value);
uint16_t getNumPagesFromValue(const Value &value);

////////////////////////////////////////////////////////////////////////////////

Value MakeRangeValue(uint16_t from, uint16_t to);
Value MakeEnumDefinitionValue(uint8_t enumValue, uint8_t enumDefinition);

////////////////////////////////////////////////////////////////////////////////

inline Value IntegerValue(int32_t value) { return Value((int)value, VALUE_TYPE_INT32); }
inline Value FloatValue(float value) { return Value(value, VALUE_TYPE_FLOAT); }
inline Value DoubleValue(double value) { return Value(value, VALUE_TYPE_DOUBLE); }
inline Value BooleanValue(bool value) { return Value(value, VALUE_TYPE_BOOLEAN); }
inline Value StringValue(const char *value) { return Value::makeStringRef(value, -1, 0); }

template<class T, uint32_t ARRAY_TYPE>
struct ArrayOf {
    Value value;

    ArrayOf(size_t size) {
        value = Value::makeArrayRef((uint32_t)size, ARRAY_TYPE, 0);
    }

    ArrayOf(Value value_) : value(value_) {}

    operator Value() const { return value; }

    operator bool() const { return value.isArray(); }

    size_t size() {
        return (size_t)value.getArray()->arraySize;
    }

    T at(int position) {
        return value.getArray()->values[position];
    }

    void at(int position, const T &point) {
        value.getArray()->values[position] = point.value;
    }
};

struct ArrayOfInteger {
    Value value;

    ArrayOfInteger(size_t size) {
        value = Value::makeArrayRef((uint32_t)size, flow::defs_v3::ARRAY_TYPE_INTEGER, 0);
    }

    ArrayOfInteger(Value value_) : value(value_) {}

    operator Value() const { return value; }

    operator bool() const { return value.isArray(); }

    size_t size() {
        return (size_t)value.getArray()->arraySize;
    }

    int at(int position) {
        return value.getArray()->values[position].getInt();
    }

    void at(int position, int intValue) {
        value.getArray()->values[position] = Value(intValue, VALUE_TYPE_INT32);
    }
};

struct ArrayOfFloat {
    Value value;

    ArrayOfFloat(size_t size) {
        value = Value::makeArrayRef((uint32_t)size, flow::defs_v3::ARRAY_TYPE_INTEGER, 0);
    }

    ArrayOfFloat(Value value_) : value(value_) {}

    operator Value() const { return value; }

    operator bool() const { return value.isArray(); }

    size_t size() {
        return (size_t)value.getArray()->arraySize;
    }

    float at(int position) {
        return value.getArray()->values[position].getFloat();
    }

    void at(int position, float floatValue) {
        value.getArray()->values[position] = Value(floatValue, VALUE_TYPE_FLOAT);
    }
};

struct ArrayOfDouble {
    Value value;

    ArrayOfDouble(size_t size) {
        value = Value::makeArrayRef((uint32_t)size, flow::defs_v3::ARRAY_TYPE_INTEGER, 0);
    }

    ArrayOfDouble(Value value_) : value(value_) {}

    operator Value() const { return value; }

    operator bool() const { return value.isArray(); }

    size_t size() {
        return (size_t)value.getArray()->arraySize;
    }

    double at(int position) {
        return value.getArray()->values[position].getDouble();
    }

    void at(int position, double doubleValue) {
        value.getArray()->values[position] = Value(doubleValue, VALUE_TYPE_DOUBLE);
    }
};

struct ArrayOfBoolean {
    Value value;

    ArrayOfBoolean(size_t size) {
        value = Value::makeArrayRef((uint32_t)size, flow::defs_v3::ARRAY_TYPE_INTEGER, 0);
    }

    ArrayOfBoolean(Value value_) : value(value_) {}

    operator Value() const { return value; }

    operator bool() const { return value.isArray(); }

    size_t size() {
        return (size_t)value.getArray()->arraySize;
    }

    bool at(int position) {
        return value.getArray()->values[position].getBoolean();
    }

    void at(int position, bool boolValue) {
        value.getArray()->values[position] = Value(boolValue, VALUE_TYPE_BOOLEAN);
    }
};

struct ArrayOfString {
    Value value;

    ArrayOfString(size_t size) {
        value = Value::makeArrayRef((uint32_t)size, flow::defs_v3::ARRAY_TYPE_INTEGER, 0);
    }

    ArrayOfString(Value value_) : value(value_) {}

    operator Value() const { return value; }

    operator bool() const { return value.isArray(); }

    size_t size() {
        return (size_t)value.getArray()->arraySize;
    }

    const char *at(int position) {
        return value.getArray()->values[position].getString();
    }

    void at(int position, const char *stringValue) {
        value.getArray()->values[position] = Value(stringValue, VALUE_TYPE_STRING);
    }
};

} // namespace eez
