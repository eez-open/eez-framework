/*
 * eez-framework tests: expressions that need more than a case of
 * expression_cases.py: the evaluation stack across evaluations, iterators,
 * hooks, the fake clock, shown pages, and instructions Studio only emits for
 * other project types.
 */

#include <math.h>
#include <string.h>

#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/expression.h>
#include <eez/flow/hooks.h>
#include <eez/flow/date.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/components/set_variable.h>
#include <eez/gui/gui.h>

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

namespace {

int requireCase(const char *expression) {
    int caseIndex = expr::findCase(expression);
    if (caseIndex == -1 || expr::componentIndex(caseIndex) == -1) {
        test::fail(__FILE__, __LINE__, test::format("no case \"%s\"", expression));
        throw test::Abort();
    }
    return caseIndex;
}

bool evalCase(const char *expression, Value &result, const int32_t *iterators = nullptr) {
    harness::clearFlowRecords();
    bool ok = expr::eval(requireCase(expression), result, iterators);
    result = result.getValue();
    return ok;
}

// the instructions of a case with an operation inserted before the END
std::vector<uint8_t> withOperation(std::vector<uint8_t> instructions, int operation) {
    uint16_t instruction = EXPR_EVAL_INSTRUCTION_TYPE_OPERATION | operation;
    instructions.insert(instructions.end() - 2, { (uint8_t)(instruction & 0xFF), (uint8_t)(instruction >> 8) });
    return instructions;
}

const uint16_t OPERATION_FLOW_TRANSLATE = 30;

double g_dateNow;

double dateNowHook() {
    return g_dateNow;
}

FlowState *pageFlowState(int pageId) {
    return getPageFlowState(g_mainAssets, pageId - 1, gui::WidgetCursor());
}

void showPage(int pageId) {
    harness::appContext()->showPage(pageId);
    harness::frame(3);
}

bool anyErrorContains(const char *text) {
    for (auto &error : harness::flowErrors()) {
        if (error.message.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// the evaluation stack

TEST("expressions/stack: an error inside an expression leaves the stack as it was") {
    Value value;
    size_t sp = g_stack.sp;
    CHECK(!evalCase("g_int_array[5]", value));
    CHECK_EQ((int)g_stack.sp, (int)sp);
    CHECK(evalCase("1 + 2", value));
    CHECK_EQ(value.getInt(), 3);
}

TEST("expressions/stack: an assignable expression after an error") {
    // the "result" local variable of a case component, after an error that
    // doesn't leave anything on the stack
    Value value;
    CHECK(!evalCase("g_int_array[5]", value));

    int caseIndex = requireCase("1 + 2");
    auto component = (SetVariableActionComponent *)expr::flowState()->flow->components[expr::componentIndex(caseIndex)];
    Value dst;
    REQUIRE(evalAssignableExpression(expr::flowState(), expr::componentIndex(caseIndex), component->entries[1]->variable, dst, FlowError::Plain("assign")));
    CHECK_EQ(dst.getType(), VALUE_TYPE_VALUE_PTR);
}

TEST("expressions/stack: an error inside Math.min does not break the next assignable expression") {
    // an error argument of Math.min must not leave the other arguments on the
    // evaluation stack: the next assignable expression needs it as it was (O1)
    Value value;
    CHECK(!evalCase("Math.min(g_int_array[5], 1) + 1", value));

    int caseIndex = requireCase("1 + 2");
    auto component = (SetVariableActionComponent *)expr::flowState()->flow->components[expr::componentIndex(caseIndex)];
    Value dst;
    bool ok = evalAssignableExpression(expr::flowState(), expr::componentIndex(caseIndex), component->entries[1]->variable, dst, FlowError::Plain("assign"));
    CHECK(ok);
}

////////////////////////////////////////////////////////////////////////////////
// Flow.index and $index

TEST("expressions/Flow.index: reads the iterators of the evaluation") {
    const int32_t iterators[MAX_ITERATORS] = { 7, 8, 9, 10 };
    Value value;

    REQUIRE(evalCase("Flow.index(1)", value, iterators));
    CHECK_EQ(value.getType(), VALUE_TYPE_INT32);
    CHECK_EQ(value.getInt(), 8);

    REQUIRE(evalCase("$index", value, iterators));
    CHECK_EQ(value.getInt(), 7);
}

TEST("expressions/Flow.index: an iterator index out of range is an error") {
    const int32_t iterators[MAX_ITERATORS] = { 7, 8, 9, 10 };
    Value value;
    size_t sp = g_stack.sp;
    CHECK(!evalCase("Flow.index(5)", value, iterators));
    CHECK_EQ((int)g_stack.sp, (int)sp);
    CHECK(!harness::flowErrors().empty());
}

////////////////////////////////////////////////////////////////////////////////
// System.getTick (the fake clock)

TEST("expressions/System.getTick: the clock") {
    Value value;
    REQUIRE(evalCase("System.getTick()", value));
    CHECK_EQ(value.getType(), VALUE_TYPE_UINT32);
    CHECK_EQ((unsigned long)value.getUInt32(), (unsigned long)harness::now());

    harness::advance(1234);
    REQUIRE(evalCase("System.getTick()", value));
    CHECK_EQ((unsigned long)value.getUInt32(), (unsigned long)harness::now());
}

TEST("expressions/System.getTick: tick / 1000") {
    Value value;
    REQUIRE(evalCase("System.getTick() / 1000", value));
    CHECK_EQ(value.getType(), VALUE_TYPE_DOUBLE);
    CHECK_NEAR(value.getDouble(), harness::now() / 1000.0, 1e-9);
}

TEST("expressions/System.getTick: tick / 1000 after 24.8 days of uptime") {
    // the uint32 tick is divided as a 64-bit integer (O10: as int32 the result
    // was negative above 2^31 ms)
    harness::advance(0x80000000u);
    Value value;
    REQUIRE(evalCase("System.getTick() / 1000", value));
    CHECK_NEAR(value.toDouble(), harness::now() / 1000.0, 1e-6);
}

////////////////////////////////////////////////////////////////////////////////
// Date.now (getDateNowHook, time zone, DST)

TEST("expressions/Date.now: the date of getDateNowHook") {
    g_dateNow = 1710498030456.0; // 2024-03-15T10:20:30.456Z
    getDateNowHook = dateNowHook;

    Value value;
    REQUIRE(evalCase("Date.now()", value));
    CHECK_EQ(value.getType(), VALUE_TYPE_DATE);
    CHECK_EQ(value.getDouble(), 1710498030456.0);

    REQUIRE(evalCase("Date.getHours(Date.now())", value));
    CHECK_EQ(value.getInt(), 10);
}

TEST("expressions/Date.now: the time zone is added") {
    g_dateNow = 1710498030456.0; // 2024-03-15T10:20:30.456Z
    getDateNowHook = dateNowHook;
    date::g_timeZone = 200; // UTC+2:00

    Value value;
    REQUIRE(evalCase("Date.getHours(Date.now())", value));
    CHECK_EQ(value.getInt(), 12);
}

TEST("expressions/Date.now: European DST starts on the last Sunday of March") {
    // O8: timeChangeRuleToLocal added the day offset in seconds to a value in
    // ms, so DST started around the 1st of April
    g_dateNow = 1711886400000.0; // 2024-03-31T12:00:00Z, DST since 01:00 UTC
    getDateNowHook = dateNowHook;
    date::g_dstRule = date::DST_RULE_EUROPE;

    Value value;
    REQUIRE(evalCase("Date.getHours(Date.now())", value));
    CHECK_EQ(value.getInt(), 13);
}

TEST("expressions/Date.now: European DST in July") {
    g_dateNow = 1719835200000.0; // 2024-07-01T12:00:00Z
    getDateNowHook = dateNowHook;
    date::g_dstRule = date::DST_RULE_EUROPE;

    Value value;
    REQUIRE(evalCase("Date.getHours(Date.now())", value));
    CHECK_EQ(value.getInt(), 13);
}

////////////////////////////////////////////////////////////////////////////////
// Flow.translate, Flow.languages

TEST("expressions/Flow.translate: the selected language") {
    Value value;
    REQUIRE(evalCase("Flow.translate(\"expr_hello\")", value));
    CHECK_STR(value.getString(), "Hello");

    g_selectedLanguage = 1;
    REQUIRE(evalCase("Flow.translate(\"expr_hello\")", value));
    CHECK_STR(value.getString(), "Hallo");
    REQUIRE(evalCase("T\"expr_bye\"", value));
    CHECK_STR(value.getString(), "Tschuess");
}

TEST("expressions/Flow.translate: no language selected gives an empty string") {
    g_selectedLanguage = 5;
    Value value;
    REQUIRE(evalCase("Flow.translate(\"expr_hello\")", value));
    CHECK_STR(value.getString(), "");
}

// Studio passes a string to Flow.translate only in LVGL builds (text resource
// IDs). These tests run the operation on the instructions of other cases.

TEST("expressions/Flow.translate: a computed string argument stays valid") {
    // the result keeps the string of the argument alive (O6: it pointed into
    // the StringRef of the argument after it was freed)
    int caseIndex = requireCase("\"expr_tr_a\" + \"expr_tr_b\"");
    auto instructions = withOperation(expr::instructions(caseIndex), OPERATION_FLOW_TRANSLATE);
    Value value;
    REQUIRE(expr::evalInstructions(caseIndex, instructions.data(), value));
    REQUIRE(value.isString());
    CHECK_STR(value.getString(), "expr_tr_aexpr_tr_b");
}

TEST("expressions/Flow.translate: a string variable argument is read") {
    // the argument is resolved with getValue() (O6: a variable (VALUE_PTR) went
    // to toInt32, which failed on "text")
    int caseIndex = requireCase("g_string + \"!\"");
    auto instructions = expr::instructions(caseIndex);
    // keep the first instruction (push g_string) and the END
    std::vector<uint8_t> pushVariable = { instructions[0], instructions[1], instructions[instructions.size() - 2], instructions[instructions.size() - 1] };
    auto translate = withOperation(pushVariable, OPERATION_FLOW_TRANSLATE);
    Value value;
    REQUIRE(expr::evalInstructions(caseIndex, translate.data(), value));
    REQUIRE(value.isString());
    CHECK_STR(value.getString(), "text");
}

////////////////////////////////////////////////////////////////////////////////
// Flow.getBitmapIndex, Flow.isPageActive

TEST("expressions/Flow.getBitmapIndex: the bitmap id in the main assets") {
    Value value;
    REQUIRE(evalCase("Flow.getBitmapIndex(\"expr_dot\")", value));
    CHECK_EQ(value.getType(), VALUE_TYPE_INT32);
    CHECK_EQ(value.getInt(), (int)gui::BITMAP_ID_EXPR_DOT);
}

TEST("expressions/Flow.isPageActive: true while the page is shown") {
    showPage(gui::PAGE_ID_EXPR_ACTIVE);
    auto fs = pageFlowState(gui::PAGE_ID_EXPR_ACTIVE);
    REQUIRE(fs != nullptr);
    CHECK(harness::flowErrors().empty());
    // its "page open" event ran SetVariable: active = Flow.isPageActive()
    Value active = fs->values[fs->flow->componentInputs.count + 0];
    CHECK_EQ(active.getType(), VALUE_TYPE_BOOLEAN);
    CHECK(active.getBoolean());
}

////////////////////////////////////////////////////////////////////////////////
// O14 in the main assets: SetColorTheme and SelectLanguage with an expression
// that is not a string

TEST("expressions/SetColorTheme: a theme that is not a string is reported") {
    // the theme is looked up by its text (O14: strcmp(themeName, NULL))
    showPage(gui::PAGE_ID_EXPR_BAD_THEME);
    CHECK(anyErrorContains("Unknown theme"));
}

TEST_BUG("expressions/SelectLanguage: a language that is not a string is reported", "O14") {
    // strcmp(languageID, NULL)
    showPage(gui::PAGE_ID_EXPR_BAD_LANGUAGE);
    CHECK(anyErrorContains("Unknown language"));
}
