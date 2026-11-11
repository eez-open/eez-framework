/*
 * eez-framework tests: expression evaluation (flow/expression, flow/operations).
 *
 * The cases are in projects/expression_cases.py. make_fwtest.py puts every case
 * into the "expressions" page of projects/fwtest as a SetVariable component
 * (entry 0: the label "case:<n>", entry 1: the case expression) and writes the
 * expected results to generated/expression_cases.inc. One test per case.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/expression.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/components/set_variable.h>

#include "document.h"
#include "harness.h"
#include "test.h"
#include "expr_cases.h"

using namespace eez;
using namespace eez::flow;

namespace eez {
namespace flow {
extern EvalStack g_stack;
}
}

namespace expr {

namespace {

const Case CASES[] = {
#include "expression_cases.inc"
};

const int NUM_CASES = (int)(sizeof(CASES) / sizeof(CASES[0]));

SetVariableActionComponent *setVariableComponent(FlowState *fs, int index) {
    auto component = fs->flow->components[index];
    if (component->type != defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
        return nullptr;
    }
    return (SetVariableActionComponent *)component;
}

bool evaluateEntry(FlowState *fs, int index, int entryIndex, Value &result, const int32_t *iterators = nullptr) {
    auto component = setVariableComponent(fs, index);
    return evalExpression(fs, index, component->entries[entryIndex]->value, result, FlowError::Plain("expression test"), nullptr, iterators);
}

// component index of every case, found by the labels (entry 0)
const std::vector<int> &caseComponentIndexes() {
    static std::vector<int> g_indexes;
    if (g_indexes.empty()) {
        auto fs = flowState();
        g_indexes.assign(NUM_CASES, -1);
        for (int i = 0; i < (int)fs->flow->components.count; i++) {
            if (!setVariableComponent(fs, i)) {
                continue;
            }
            Value label;
            int caseIndex;
            if (evaluateEntry(fs, i, 0, label) && label.isString() &&
                sscanf(label.getString(), "case:%d", &caseIndex) == 1 && caseIndex >= 0 && caseIndex < NUM_CASES) {
                g_indexes[caseIndex] = i;
            }
        }
    }
    return g_indexes;
}

bool isStringType(ValueType type) {
    return type == VALUE_TYPE_STRING || type == VALUE_TYPE_STRING_REF || type == VALUE_TYPE_STRING_ASSET;
}

bool isArrayType(ValueType type) {
    return type == VALUE_TYPE_ARRAY || type == VALUE_TYPE_ARRAY_REF || type == VALUE_TYPE_ARRAY_ASSET;
}

bool isNumberType(ValueType type) {
    return (type >= VALUE_TYPE_INT8 && type <= VALUE_TYPE_UINT64) || type == VALUE_TYPE_FLOAT || type == VALUE_TYPE_DOUBLE;
}

std::string describe(const Value &value) {
    return test::format("type %d, %s", (int)value.getType(), encode(value).c_str());
}

// expected NaN and infinities are compared exactly, the rest with a relative tolerance
void checkNumber(double actual, double expected, double relativeTolerance, const char *file, int line) {
    if (isnan(expected)) {
        if (!isnan(actual)) {
            test::fail(file, line, test::format("%.17g, expected nan", actual));
        }
    } else if (isinf(expected) || isinf(actual) || isnan(actual)) {
        if (!(actual == expected)) {
            test::fail(file, line, test::format("%.17g, expected %.17g", actual, expected));
        }
    } else if (fabs(actual - expected) > relativeTolerance * (1 + fabs(expected))) {
        test::fail(file, line, test::format("%.17g, expected %.17g", actual, expected));
    }
}

void runCase(int caseIndex) {
    const Case &c = CASES[caseIndex];

    auto fs = flowState();
    REQUIRE(fs != nullptr);
    REQUIRE(componentIndex(caseIndex) != -1);

    harness::clearFlowRecords();
    size_t sp = g_stack.sp;

    Value value;
    bool ok = eval(caseIndex, value);

    // the evaluation stack is balanced whatever the result
    CHECK_EQ((int)g_stack.sp, (int)sp);

    if (c.check == CASE_ANY) {
        return;
    }

    if (c.check == CASE_ERROR) {
        if (ok) {
            test::fail(__FILE__, __LINE__, "no error, the result is " + describe(value));
            return;
        }
        CHECK(!harness::flowErrors().empty());
        if (c.string) {
            bool found = false;
            for (auto &error : harness::flowErrors()) {
                if (error.message.find(c.string) != std::string::npos) {
                    found = true;
                }
            }
            if (!found) {
                for (auto &error : harness::flowErrors()) {
                    test::fail(__FILE__, __LINE__, "error message \"" + error.message + "\" does not contain \"" + c.string + "\"");
                }
            }
        }
        return;
    }

    if (!ok) {
        for (auto &error : harness::flowErrors()) {
            test::fail(__FILE__, __LINE__, "evaluation failed: " + error.message);
        }
        REQUIRE(ok);
    }

    value = value.getValue();
    auto type = value.getType();

    if (c.check == CASE_NUMBER) {
        if (!isNumberType(type)) {
            test::fail(__FILE__, __LINE__, describe(value) + ", expected a number");
            return;
        }
        checkNumber(value.toDouble(), c.number, 1e-6, __FILE__, __LINE__);
    } else if (c.check == CASE_PREFIX) {
        if (!isStringType(type)) {
            test::fail(__FILE__, __LINE__, describe(value) + ", expected a string");
            return;
        }
        const char *str = value.getString();
        if (strncmp(str, c.string, strlen(c.string)) != 0) {
            test::fail(__FILE__, __LINE__, test::format("\"%s\" does not start with \"%s\"", str, c.string));
        }
    } else if (c.check == CASE_ARRAY) {
        if (!isArrayType(type)) {
            test::fail(__FILE__, __LINE__, describe(value) + ", expected an array");
            return;
        }
        CHECK_EQ((int)value.getArray()->arraySize, (int)c.number);
        if (c.string) {
            std::string elements = encode(value);
            CHECK_STR(elements.c_str(), c.string);
        }
    } else if (c.type == VALUE_TYPE_STRING) {
        if (!isStringType(type)) {
            test::fail(__FILE__, __LINE__, describe(value) + ", expected a string");
            return;
        }
        CHECK_STR(value.getString(), c.string);
    } else {
        if (type != c.type) {
            test::fail(__FILE__, __LINE__, describe(value) + test::format(", expected type %d", (int)c.type));
            return;
        }
        if (c.type == VALUE_TYPE_BOOLEAN) {
            CHECK_EQ(value.getBoolean(), c.number != 0);
        } else if (c.type == VALUE_TYPE_FLOAT) {
            checkNumber(value.getFloat(), c.number, 1e-6, __FILE__, __LINE__);
        } else if (c.type == VALUE_TYPE_DOUBLE) {
            checkNumber(value.getDouble(), c.number, 1e-12, __FILE__, __LINE__);
        } else if (c.type == VALUE_TYPE_DATE) {
            CHECK_EQ(value.getDouble(), c.number);
        } else if (c.type == VALUE_TYPE_INT64) {
            CHECK_EQ((long long)value.getInt64(), (long long)c.number);
        } else if (c.type == VALUE_TYPE_UINT32) {
            CHECK_EQ((unsigned long)value.getUInt32(), (unsigned long)c.number);
        } else if (c.type == VALUE_TYPE_INT32) {
            CHECK_EQ(value.getInt32(), (int)c.number);
        }
    }
}

struct Register {
    Register() {
        for (int i = 0; i < NUM_CASES; i++) {
            test::registerTest(
                test::format("expressions: %s", CASES[i].expression),
                [i] { runCase(i); },
                CASES[i].knownBug
            );
        }
    }
} g_register;

} // namespace

int caseCount() {
    return NUM_CASES;
}

const Case &getCase(int caseIndex) {
    return CASES[caseIndex];
}

int findCase(const char *expression) {
    for (int i = 0; i < NUM_CASES; i++) {
        if (strcmp(CASES[i].expression, expression) == 0) {
            return i;
        }
    }
    return -1;
}

FlowState *flowState() {
    return getPageFlowState(g_mainAssets, gui::PAGE_ID_EXPRESSIONS - 1, gui::WidgetCursor());
}

int componentIndex(int caseIndex) {
    if (caseIndex < 0 || caseIndex >= NUM_CASES) {
        return -1;
    }
    return caseComponentIndexes()[caseIndex];
}

std::vector<uint8_t> instructions(int caseIndex) {
    std::vector<uint8_t> result;
    auto component = setVariableComponent(flowState(), componentIndex(caseIndex));
    const uint8_t *p = component->entries[1]->value;
    for (;; p += 2) {
        result.push_back(p[0]);
        result.push_back(p[1]);
        uint16_t instruction = p[0] | (p[1] << 8);
        if ((instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK) == EXPR_EVAL_INSTRUCTION_TYPE_END) {
            break;
        }
    }
    return result;
}

bool eval(int caseIndex, Value &result, const int32_t *iterators) {
    return evaluateEntry(flowState(), componentIndex(caseIndex), 1, result, iterators);
}

bool evalInstructions(int caseIndex, const uint8_t *instructions, Value &result, const int32_t *iterators) {
    return evalExpression(flowState(), componentIndex(caseIndex), instructions, result, FlowError::Plain("expression test"), nullptr, iterators);
}

std::string encode(const Value &value0) {
    Value value = value0.getValue();
    switch (value.getType()) {
    case VALUE_TYPE_UNDEFINED: return "undefined";
    case VALUE_TYPE_NULL: return "null";
    case VALUE_TYPE_BOOLEAN: return value.getBoolean() ? "true" : "false";
    case VALUE_TYPE_INT8: return test::format("%d", (int)value.getInt8());
    case VALUE_TYPE_UINT8: return test::format("%d", (int)value.getUInt8());
    case VALUE_TYPE_INT16: return test::format("%d", (int)value.getInt16());
    case VALUE_TYPE_UINT16: return test::format("%d", (int)value.getUInt16());
    case VALUE_TYPE_INT32: return test::format("%d", (int)value.getInt32());
    case VALUE_TYPE_UINT32: return test::format("%u", (unsigned)value.getUInt32());
    case VALUE_TYPE_INT64: return test::format("%lld", (long long)value.getInt64());
    case VALUE_TYPE_UINT64: return test::format("%llu", (unsigned long long)value.getUInt64());
    case VALUE_TYPE_FLOAT: return test::format("f:%.9g", (double)value.getFloat());
    case VALUE_TYPE_DOUBLE: return test::format("d:%.15g", value.getDouble());
    case VALUE_TYPE_DATE: return test::format("date:%.0f", value.getDouble());
    case VALUE_TYPE_ERROR: return "error";
    default: break;
    }
    if (value.isString()) {
        const char *str = value.getString();
        return std::string("\"") + (str ? str : "(null)") + "\"";
    }
    if (value.isArray()) {
        auto array = value.getArray();
        std::string result = "[";
        for (uint32_t i = 0; i < array->arraySize; i++) {
            if (i > 0) {
                result += ",";
            }
            result += encode(array->values[i]);
        }
        return result + "]";
    }
    return test::format("type:%d", (int)value.getType());
}

} // namespace expr

TEST("expressions: every case component is in the flow") {
    auto fs = expr::flowState();
    REQUIRE(fs != nullptr);
    // the "page open" event and one SetVariable per case
    CHECK_EQ((int)fs->flow->components.count, 1 + expr::caseCount());
    for (int i = 0; i < expr::caseCount(); i++) {
        if (expr::componentIndex(i) == -1) {
            test::fail(__FILE__, __LINE__, test::format("case %d not found", i));
        }
    }
}
