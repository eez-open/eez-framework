#include <vector>
#include <eez/core/assets.h>
#include <eez/flow/expression.h>
#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include "test.h"
#include "harness.h"
#include "expr_cases.h"

using namespace eez;
using namespace eez::flow;

namespace {
void word(std::vector<uint8_t> &bytes, uint16_t value) {
    bytes.push_back(value & 255); bytes.push_back(value >> 8);
}
std::vector<uint8_t> list(std::initializer_list<uint16_t> targets) {
    std::vector<uint8_t> bytes;
    word(bytes, EXPR_EVAL_ASSIGNABLE_LIST); word(bytes, targets.size());
    word(bytes, 8 + 6 * targets.size()); word(bytes, 0);
    for (auto target : targets) {
        word(bytes, 2); word(bytes, target); word(bytes, EXPR_EVAL_INSTRUCTION_TYPE_END);
    }
    return bytes;
}
bool evaluate(const std::vector<uint8_t> &bytes, Value &value, int *consumed = nullptr) {
    const int at = expr::findCase("42");
    return evalAssignableListExpression(expr::flowState(), expr::componentIndex(at), bytes.data(), bytes.size(), value,
        FlowError::Plain("assignable list"), consumed);
}
}

TEST("expressions/assignable-list: native and ordinary references are not dereferenced") {
    auto state = expr::flowState();
    REQUIRE(state && state->assets->flowDefinition->globalVariables.count > 0);
    const auto native = EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR | state->assets->flowDefinition->globalVariables.count;
    auto bytes = list({(uint16_t)native, EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR});
    Value value;
    int consumed = 0;
    REQUIRE(evaluate(bytes, value, &consumed));
    REQUIRE(value.isArray());
    CHECK_EQ(value.getArray()->arraySize, 2U);
    CHECK_EQ(value.getArray()->values[0].getType(), VALUE_TYPE_NATIVE_VARIABLE);
    CHECK_EQ(value.getArray()->values[0].getInt(), 1);
    CHECK_EQ(value.getArray()->values[1].getType(), VALUE_TYPE_VALUE_PTR);
    CHECK_EQ(consumed, (int)bytes.size());
}

TEST("expressions/assignable-list: empty list and independent result ownership") {
    Value first, second;
    REQUIRE(evaluate(list({}), first));
    REQUIRE(evaluate(list({}), second));
    REQUIRE(first.isArray() && second.isArray());
    CHECK_EQ(first.getArray()->arraySize, 0U);
    CHECK(first.getArray() != second.getArray());
}

TEST("expressions/assignable-list: invalid item returns no partially evaluated list") {
    harness::clearFlowRecords();
    const auto native = EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR | expr::flowState()->assets->flowDefinition->globalVariables.count;
    Value result(123);
    REQUIRE(!evaluate(list({(uint16_t)native, EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT}), result));
    CHECK_EQ(result.getType(), VALUE_TYPE_UNDEFINED);
    CHECK_EQ(harness::flowErrors().size(), 1U);
}

TEST("expressions/assignable-list: malformed item length is diagnosed") {
    auto bytes = list({EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR});
    bytes[8] = 3; // Actual expression has two words.
    harness::clearFlowRecords();
    Value result;
    REQUIRE(!evaluate(bytes, result));
    CHECK_EQ(result.getType(), VALUE_TYPE_UNDEFINED);
    REQUIRE(harness::flowErrors().size() == 1);
    CHECK(harness::flowErrors()[0].message.find("span") != std::string::npos);
}

TEST("expressions/assignable-list: all truncated prefixes fail before evaluation") {
    const auto valid = list({EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR});
    for (size_t size = 0; size < valid.size(); ++size) {
        std::vector<uint8_t> bytes(valid.begin(), valid.begin() + size);
        harness::clearFlowRecords();
        Value value;
        CHECK(!evaluate(bytes, value));
        CHECK_EQ(value.getType(), VALUE_TYPE_UNDEFINED);
    }
}

TEST("expressions/assignable-list: missing END cannot consume the following item") {
    auto bytes = list({EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR, EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR});
    bytes[12] = 0; bytes[13] = 0; // Replace first END with PUSH_CONSTANT.
    harness::clearFlowRecords();
    Value value;
    CHECK(!evaluate(bytes, value));
    CHECK_EQ(value.getType(), VALUE_TYPE_UNDEFINED);
}
