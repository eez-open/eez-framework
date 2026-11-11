/*
 * eez-framework tests: the expression cases (projects/expression_cases.py) for
 * the tests of this folder.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/value.h>

namespace eez {
namespace flow {
struct FlowState;
}
}

namespace expr {

enum CaseCheck {
    CASE_ANY,     // only that the evaluation finishes with a balanced stack
    CASE_VALUE,   // type and value
    CASE_NUMBER,  // any numeric type with the value
    CASE_PREFIX,  // a string that starts with the expected string
    CASE_ERROR,   // the evaluation fails (string: a text of the error message)
    CASE_ARRAY    // an array (number: length, string: encoded elements)
};

struct Case {
    const char *expression;
    CaseCheck check;
    eez::ValueType type;
    double number;
    const char *string;
    const char *knownBug;
};

int caseCount();
const Case &getCase(int caseIndex);

// index of the case with this expression, -1 if there is none
int findCase(const char *expression);

// the flow state of the "expressions" page
eez::flow::FlowState *flowState();

// index of the SetVariable component of the case in the "expressions" flow
int componentIndex(int caseIndex);

// the instructions of the case expression (entry 1 of its component), with the
// END instruction
std::vector<uint8_t> instructions(int caseIndex);

// evaluates the case expression like a component property
bool eval(int caseIndex, eez::Value &result, const int32_t *iterators = nullptr);

// evaluates other instructions in the context of the case component
bool evalInstructions(int caseIndex, const uint8_t *instructions, eez::Value &result, const int32_t *iterators = nullptr);

// the text of a value as the array check of the cases writes it
// (make_fwtest.py encode_value)
std::string encode(const eez::Value &value);

} // namespace expr
