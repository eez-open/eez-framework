/*
 * eez-framework tests: the performance changes of the expression evaluator
 * (flow/expression.cpp, flow/operations.cpp):
 *
 * - the jump instructions of ?:, && and || (short-circuit evaluation, see
 *   EXPR_EVAL_INSTRUCTION_TYPE_JUMP in core/assets.h). Studio doesn't emit them
 *   for the test project yet, so the tests evaluate hand-assembled instructions
 *   with the constants and globals of the project and compare the results with
 *   the CONDITIONAL, LOGICAL_AND and LOGICAL_OR operations (the encoding without
 *   jumps, which the evaluator still runs);
 * - the allocations of String.format and of a string +.
 *
 * Some native variables of parts/expr_extra.py are replaced (in
 * g_dataOperationsFunctions) by counting ones, to see which operands are read.
 *
 * The allocations are counted with the malloc hook of the sanitizers
 * (__sanitizer_install_malloc_and_free_hooks): in the --no-sanitize build only
 * the results are checked.
 */

#include <stdio.h>
#include <string.h>

#include <functional>
#include <string>
#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/core/value.h>
#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/expression.h>
#include <eez/flow/flow_defs_v3.h>
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

extern "C" int __sanitizer_install_malloc_and_free_hooks(
    void (*mallocHook)(const volatile void *, size_t),
    void (*freeHook)(const volatile void *)) __attribute__((weak));

namespace {

////////////////////////////////////////////////////////////////////////////////
// allocation counting (sanitizer builds)

thread_local bool t_countAllocs;
thread_local int t_allocs;

void mallocHook(const volatile void *, size_t) {
    if (t_countAllocs) {
        t_allocs++;
    }
}

void freeHook(const volatile void *) {
}

bool canCountAllocs() {
    static int g_installed = -1;
    if (g_installed == -1) {
        g_installed = __sanitizer_install_malloc_and_free_hooks && __sanitizer_install_malloc_and_free_hooks(mallocHook, freeHook) ? 1 : 0;
    }
    return g_installed == 1;
}

int countAllocs(const std::function<void()> &f) {
    t_allocs = 0;
    t_countAllocs = true;
    f();
    t_countAllocs = false;
    return t_allocs;
}

////////////////////////////////////////////////////////////////////////////////
// counting native variables

struct Native {
    int dataId;
    Value value;
    int calls;
    gui::DataOperationsFunction saved;
};

// NAT_TRUE, NAT_FALSE, NAT_ERROR: values of a test or a left operand read
// through getValue; NAT_COUNT: the operand that must not be read when it is
// skipped; NAT_STR: a format string for String.format
enum { NAT_TRUE, NAT_FALSE, NAT_ERROR, NAT_COUNT, NAT_STR, NUM_NATIVES };

Native g_natives[NUM_NATIVES] = {
    { gui::DATA_ID_EXPR_NAT_U32, Value(), 0, nullptr },
    { gui::DATA_ID_EXPR_NAT_I64, Value(), 0, nullptr },
    { gui::DATA_ID_EXPR_NAT_U8, Value(), 0, nullptr },
    { gui::DATA_ID_EXPR_NAT_I8, Value(), 0, nullptr },
    { gui::DATA_ID_EXPR_NAT_I16, Value(), 0, nullptr },
};

template <int N>
void nativeGet(gui::DataOperationEnum operation, const gui::WidgetCursor &, Value &value) {
    if (operation == gui::DATA_OPERATION_GET) {
        g_natives[N].calls++;
        value = g_natives[N].value;
    }
}

// replaces the native variables while it lives
struct Natives {
    Natives() {
        g_natives[NAT_TRUE].value = Value(true, VALUE_TYPE_BOOLEAN);
        g_natives[NAT_FALSE].value = Value(false, VALUE_TYPE_BOOLEAN);
        g_natives[NAT_ERROR].value = Value::makeError();
        g_natives[NAT_COUNT].value = Value(7, VALUE_TYPE_INT32);
        g_natives[NAT_STR].value = Value("%d");
        install(NAT_TRUE, nativeGet<NAT_TRUE>);
        install(NAT_FALSE, nativeGet<NAT_FALSE>);
        install(NAT_ERROR, nativeGet<NAT_ERROR>);
        install(NAT_COUNT, nativeGet<NAT_COUNT>);
        install(NAT_STR, nativeGet<NAT_STR>);
        resetCalls();
    }
    ~Natives() {
        for (auto &native : g_natives) {
            gui::g_dataOperationsFunctions[native.dataId] = native.saved;
            native.value = Value();
        }
    }
    static void install(int n, gui::DataOperationsFunction function) {
        g_natives[n].saved = gui::g_dataOperationsFunctions[g_natives[n].dataId];
        gui::g_dataOperationsFunctions[g_natives[n].dataId] = function;
    }
    static void resetCalls() {
        for (auto &native : g_natives) {
            native.calls = 0;
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
// instructions

typedef std::vector<uint16_t> Code;

Code operator+(Code a, const Code &b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

int requireCase(const char *expression) {
    int caseIndex = expr::findCase(expression);
    if (caseIndex == -1 || expr::componentIndex(caseIndex) == -1) {
        test::fail(__FILE__, __LINE__, test::format("no case \"%s\"", expression));
        throw test::Abort();
    }
    return caseIndex;
}

// the instructions of a case without the END instruction (the case must not
// have jumps)
Code caseCode(const char *expression) {
    auto bytes = expr::instructions(requireCase(expression));
    Code code;
    for (size_t i = 0; i + 2 < bytes.size(); i += 2) {
        code.push_back(bytes[i] | (bytes[i + 1] << 8));
    }
    return code;
}

int findConstant(const std::function<bool(const Value &)> &match, const char *what) {
    auto &constants = g_mainAssets->flowDefinition->constants;
    for (uint32_t i = 0; i < constants.count; i++) {
        if (match(*constants[i])) {
            return (int)i;
        }
    }
    test::fail(__FILE__, __LINE__, std::string("the project has no constant ") + what);
    throw test::Abort();
}

Code constant(int index) {
    return { (uint16_t)(EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT | index) };
}

Code boolConstant(bool value) {
    return constant(findConstant([value](const Value &v) { return v.getType() == VALUE_TYPE_BOOLEAN && v.getBoolean() == value; }, value ? "true" : "false"));
}

Code intConstant(int value) {
    return constant(findConstant([value](const Value &v) { return v.getType() == VALUE_TYPE_INT32 && v.getInt32() == value; }, "of type int32"));
}

Code stringConstant(const char *value) {
    return constant(findConstant([value](const Value &v) { return v.isString() && strcmp(v.getString(), value) == 0; }, value));
}

Code op(int operation) {
    return { (uint16_t)(EXPR_EVAL_INSTRUCTION_TYPE_OPERATION | operation) };
}

Code native(int n) {
    int index = (int)g_mainAssets->flowDefinition->globalVariables.count + g_natives[n].dataId - 1;
    return { (uint16_t)(EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR | index) };
}

// the global variable a case starts with (e.g. "+g_int")
Code caseGlobal(const char *expression) {
    Code code = caseCode(expression);
    REQUIRE(!code.empty() && (code[0] & EXPR_EVAL_INSTRUCTION_TYPE_MASK) == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR);
    return { code[0] };
}

int globalIndex(const Code &global) {
    return global[0] & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
}

// the value a PUSH_GLOBAL_VAR of the main assets points to
Value *globalValue(const Code &global) {
    int index = globalIndex(global);
    return g_globalVariables ? &g_globalVariables->values[index] : g_mainAssets->flowDefinition->globalVariables[index];
}

// a test value that is an error value: -"5"
Code errorValue() {
    return stringConstant("5") + op(defs_v3::OPERATION_TYPE_UNARY_MINUS);
}

// a value whose toBool fails: a flow output
Code flowOutput() {
    return { (uint16_t)(EXPR_EVAL_INSTRUCTION_TYPE_PUSH_OUTPUT | 0) };
}

// t ? c : a  ->  [t] [JUMP_IF_FALSE off1] [c] [JUMP off2] [a]
Code jumpConditional(const Code &t, const Code &c, const Code &a) {
    return t + Code{ EXPR_EVAL_INSTRUCTION_JUMP_IF_FALSE, (uint16_t)c.size() } + c + Code{ EXPR_EVAL_INSTRUCTION_JUMP, (uint16_t)a.size() } + a;
}

Code opConditional(const Code &t, const Code &c, const Code &a) {
    return t + c + a + op(defs_v3::OPERATION_TYPE_CONDITIONAL);
}

// a && b  ->  [a] [AND off] [b] [OPERATION LOGICAL_AND]
Code jumpAnd(const Code &a, const Code &b) {
    return a + Code{ EXPR_EVAL_INSTRUCTION_JUMP_AND, (uint16_t)(b.size() + 1) } + b + op(defs_v3::OPERATION_TYPE_LOGICAL_AND);
}

Code opAnd(const Code &a, const Code &b) {
    return a + b + op(defs_v3::OPERATION_TYPE_LOGICAL_AND);
}

Code jumpOr(const Code &a, const Code &b) {
    return a + Code{ EXPR_EVAL_INSTRUCTION_JUMP_OR, (uint16_t)(b.size() + 1) } + b + op(defs_v3::OPERATION_TYPE_LOGICAL_OR);
}

Code opOr(const Code &a, const Code &b) {
    return a + b + op(defs_v3::OPERATION_TYPE_LOGICAL_OR);
}

std::vector<uint8_t> assemble(const Code &code, const Code &end = Code{ EXPR_EVAL_INSTRUCTION_TYPE_END }) {
    std::vector<uint8_t> bytes;
    for (auto word : code + end) {
        bytes.push_back(word & 0xFF);
        bytes.push_back(word >> 8);
    }
    return bytes;
}

// the case whose component is the context of the evaluations
int contextCase() {
    return requireCase("true && false");
}

struct Result {
    bool ok;
    std::string text;
    int numInstructionBytes;
};

// evaluates like a component property, checks that the stack is balanced
Result run(const Code &code) {
    harness::clearFlowRecords();
    auto bytes = assemble(code);
    size_t sp = g_stack.sp;
    Value value;
    int numInstructionBytes = 0;
    int caseIndex = contextCase();
    bool ok = evalExpression(expr::flowState(), expr::componentIndex(caseIndex), bytes.data(), value, FlowError::Plain("perf test"), &numInstructionBytes);
    CHECK_EQ((int)g_stack.sp, (int)sp);
    // every path ends at the END instruction
    CHECK_EQ(numInstructionBytes, (int)bytes.size());
    if (ok && !harness::flowErrors().empty()) {
        test::fail(__FILE__, __LINE__, "a flow error without a failed evaluation: " + harness::flowErrors()[0].message);
    }
    return { ok, ok ? expr::encode(value) : "error", numInstructionBytes };
}

std::string describe(const Result &result) {
    return result.ok ? result.text : "error";
}

#define CHECK_SAME(JUMP_CODE, OP_CODE, WHAT) \
    do { \
        Result jumpResult = run(JUMP_CODE); \
        Result opResult = run(OP_CODE); \
        if (jumpResult.ok != opResult.ok || jumpResult.text != opResult.text) { \
            test::fail(__FILE__, __LINE__, std::string(WHAT) + ": jumps give " + describe(jumpResult) + ", the operation gives " + describe(opResult)); \
        } \
    } while (0)

struct Piece {
    const char *name;
    Code code;
};

// test values and operands: constants, indirect values (native variables),
// an error value and a value toBool can't convert
std::vector<Piece> testValues() {
    return {
        { "true", boolConstant(true) },
        { "false", boolConstant(false) },
        { "0", intConstant(0) },
        { "1", intConstant(1) },
        { "\"\"", stringConstant("") },
        { "\"0\"", stringConstant("0") },
        { "nat true", native(NAT_TRUE) },
        { "nat false", native(NAT_FALSE) },
        { "nat error", native(NAT_ERROR) },
        { "error", errorValue() },
        { "flow output", flowOutput() },
    };
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// ?:

TEST("expressions/jumps: t ? c : a with a true and a false test") {
    Natives natives;
    REQUIRE(run(jumpConditional(boolConstant(true), intConstant(1), intConstant(2))).text == "1");
    CHECK_EQ(run(jumpConditional(boolConstant(false), intConstant(1), intConstant(2))).text, std::string("2"));
    CHECK_EQ(run(jumpConditional(native(NAT_TRUE), stringConstant("a"), stringConstant("b"))).text, std::string("\"a\""));
    CHECK_EQ(run(jumpConditional(native(NAT_FALSE), stringConstant("a"), stringConstant("b"))).text, std::string("\"b\""));
}

TEST("expressions/jumps: t ? c : a with an error test is an error") {
    Natives natives;
    // an error value, an indirect error and a value toBool can't convert
    CHECK(!run(jumpConditional(errorValue(), intConstant(1), intConstant(2))).ok);
    CHECK(!run(jumpConditional(native(NAT_ERROR), intConstant(1), intConstant(2))).ok);
    CHECK(!run(jumpConditional(flowOutput(), intConstant(1), intConstant(2))).ok);
    CHECK(!harness::flowErrors().empty());
    // neither branch is read
    CHECK(!run(jumpConditional(errorValue(), native(NAT_COUNT), native(NAT_COUNT))).ok);
    CHECK_EQ(g_natives[NAT_COUNT].calls, 0);
}

TEST("expressions/jumps: t ? c : a gives what the CONDITIONAL operation gives") {
    Natives natives;
    for (auto &t : testValues()) {
        CHECK_SAME(jumpConditional(t.code, intConstant(1), stringConstant("b")), opConditional(t.code, intConstant(1), stringConstant("b")), std::string(t.name) + " ? 1 : \"b\"");
        CHECK_SAME(jumpConditional(t.code, errorValue(), intConstant(2)), opConditional(t.code, errorValue(), intConstant(2)), std::string(t.name) + " ? error : 2");
        CHECK_SAME(jumpConditional(t.code, intConstant(1), errorValue()), opConditional(t.code, intConstant(1), errorValue()), std::string(t.name) + " ? 1 : error");
    }
}

TEST("expressions/jumps: the branch of ?: that is not taken is not evaluated") {
    Natives natives;
    // the branch reads the native variable (+), CONDITIONAL alone doesn't read
    // its operands
    Code countPlusOne = native(NAT_COUNT) + intConstant(1) + op(defs_v3::OPERATION_TYPE_ADD);

    CHECK_EQ(run(jumpConditional(boolConstant(true), intConstant(2), countPlusOne)).text, std::string("2"));
    CHECK_EQ(g_natives[NAT_COUNT].calls, 0);
    CHECK_EQ(run(jumpConditional(boolConstant(false), countPlusOne, intConstant(2))).text, std::string("2"));
    CHECK_EQ(g_natives[NAT_COUNT].calls, 0);

    // the taken branch is evaluated
    CHECK_EQ(run(jumpConditional(boolConstant(false), intConstant(2), countPlusOne)).text, std::string("8"));
    CHECK_EQ(g_natives[NAT_COUNT].calls, 1);

    // without jumps both branches are evaluated
    Natives::resetCalls();
    CHECK_EQ(run(opConditional(boolConstant(true), intConstant(2), countPlusOne)).text, std::string("2"));
    CHECK_EQ(g_natives[NAT_COUNT].calls, 1);

    // an error in the branch that is not taken is not an error
    CHECK_EQ(run(jumpConditional(boolConstant(true), intConstant(1), errorValue())).text, std::string("1"));
}

TEST("expressions/jumps: ?: inside an expression continues after the conditional") {
    Natives natives;
    // (t ? 1 : 2) + 2
    Code plusTwo = intConstant(2) + op(defs_v3::OPERATION_TYPE_ADD);
    CHECK_EQ(run(jumpConditional(boolConstant(true), intConstant(1), intConstant(2)) + plusTwo).text, std::string("3"));
    CHECK_EQ(run(jumpConditional(boolConstant(false), intConstant(1), intConstant(2)) + plusTwo).text, std::string("4"));
    // an error test: the error goes on to the + (an error)
    CHECK(!run(jumpConditional(errorValue(), intConstant(1), intConstant(2)) + plusTwo).ok);
    // 2 + (t ? 1 : 2): the conditional on top of an operand
    CHECK_EQ(run(intConstant(2) + jumpConditional(boolConstant(false), intConstant(1), intConstant(2)) + op(defs_v3::OPERATION_TYPE_ADD)).text, std::string("4"));
}

TEST("expressions/jumps: nested conditionals") {
    Natives natives;
    // t1 ? (t2 ? "a" : "b") : "c" and t1 ? "a" : (t2 ? "b" : "c")
    const bool bools[] = { false, true };
    for (bool t1 : bools) {
        for (bool t2 : bools) {
            Code inner = jumpConditional(boolConstant(t2), stringConstant("a"), stringConstant("b"));
            CHECK_SAME(jumpConditional(boolConstant(t1), inner, stringConstant("c")),
                opConditional(boolConstant(t1), opConditional(boolConstant(t2), stringConstant("a"), stringConstant("b")), stringConstant("c")),
                test::format("%d ? (%d ? a : b) : c", t1, t2));
            Code inner2 = jumpConditional(boolConstant(t2), stringConstant("b"), stringConstant("c"));
            CHECK_SAME(jumpConditional(boolConstant(t1), stringConstant("a"), inner2),
                opConditional(boolConstant(t1), stringConstant("a"), opConditional(boolConstant(t2), stringConstant("b"), stringConstant("c"))),
                test::format("%d ? a : (%d ? b : c)", t1, t2));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// && and ||

TEST("expressions/jumps: && and || give what LOGICAL_AND and LOGICAL_OR give") {
    Natives natives;
    auto values = testValues();
    for (auto &a : values) {
        for (auto &b : values) {
            CHECK_SAME(jumpAnd(a.code, b.code), opAnd(a.code, b.code), std::string(a.name) + " && " + b.name);
            CHECK_SAME(jumpOr(a.code, b.code), opOr(a.code, b.code), std::string(a.name) + " || " + b.name);
        }
    }
}

TEST("expressions/jumps: && and || results") {
    Natives natives;
    CHECK_EQ(run(jumpAnd(boolConstant(true), boolConstant(false))).text, std::string("false"));
    CHECK_EQ(run(jumpAnd(boolConstant(true), intConstant(1))).text, std::string("true"));
    CHECK_EQ(run(jumpAnd(boolConstant(false), intConstant(1))).text, std::string("false"));
    CHECK_EQ(run(jumpOr(boolConstant(false), intConstant(0))).text, std::string("false"));
    CHECK_EQ(run(jumpOr(boolConstant(false), stringConstant("0"))).text, std::string("true"));
    CHECK_EQ(run(jumpOr(boolConstant(true), intConstant(0))).text, std::string("true"));
    // the result is a boolean also when the left operand decides it
    CHECK_EQ(run(jumpAnd(intConstant(0), boolConstant(true))).text, std::string("false"));
    CHECK_EQ(run(jumpOr(intConstant(1), boolConstant(false))).text, std::string("true"));
    // an error on the left is the result, an error on the right only when it
    // is needed
    CHECK(!run(jumpAnd(errorValue(), boolConstant(true))).ok);
    CHECK(!run(jumpOr(native(NAT_ERROR), boolConstant(true))).ok);
    CHECK_EQ(run(jumpAnd(boolConstant(false), errorValue())).text, std::string("false"));
    CHECK_EQ(run(jumpOr(boolConstant(true), errorValue())).text, std::string("true"));
    CHECK(!run(jumpAnd(boolConstant(true), errorValue())).ok);
    CHECK(!run(jumpOr(boolConstant(false), errorValue())).ok);
}

TEST("expressions/jumps: the right operand of && and || is read only when it decides") {
    Natives natives;
    auto &count = g_natives[NAT_COUNT].calls;

    // left decides: the right operand is not read
    CHECK_EQ(run(jumpAnd(boolConstant(false), native(NAT_COUNT))).text, std::string("false"));
    CHECK_EQ(count, 0);
    CHECK_EQ(run(jumpOr(boolConstant(true), native(NAT_COUNT))).text, std::string("true"));
    CHECK_EQ(count, 0);
    CHECK(!run(jumpAnd(errorValue(), native(NAT_COUNT))).ok);
    CHECK_EQ(count, 0);

    // right decides: read once
    CHECK_EQ(run(jumpAnd(boolConstant(true), native(NAT_COUNT))).text, std::string("true"));
    CHECK_EQ(count, 1);
    Natives::resetCalls();
    CHECK_EQ(run(jumpOr(boolConstant(false), native(NAT_COUNT))).text, std::string("true"));
    CHECK_EQ(count, 1);

    // the left operand is read once
    Natives::resetCalls();
    CHECK_EQ(run(jumpAnd(native(NAT_TRUE), boolConstant(true))).text, std::string("true"));
    CHECK_EQ(g_natives[NAT_TRUE].calls, 1);
    CHECK_EQ(run(jumpOr(native(NAT_FALSE), boolConstant(false))).text, std::string("false"));
    CHECK_EQ(g_natives[NAT_FALSE].calls, 1);

    // without jumps both operands are read
    Natives::resetCalls();
    CHECK_EQ(run(opAnd(boolConstant(false), native(NAT_COUNT))).text, std::string("false"));
    CHECK_EQ(count, 1);
}

////////////////////////////////////////////////////////////////////////////////
// nested

TEST("expressions/jumps: a ? (b && c) : d") {
    Natives natives;
    auto &count = g_natives[NAT_COUNT].calls;

    std::vector<Piece> as = { { "true", boolConstant(true) }, { "false", boolConstant(false) }, { "nat true", native(NAT_TRUE) }, { "error", errorValue() } };
    std::vector<Piece> bs = { { "true", boolConstant(true) }, { "false", boolConstant(false) }, { "error", errorValue() } };
    std::vector<Piece> cs = { { "count", native(NAT_COUNT) }, { "0", intConstant(0) }, { "error", errorValue() } };
    std::vector<Piece> ds = { { "\"c\"", stringConstant("c") }, { "count", native(NAT_COUNT) } };
    for (auto &a : as) {
        for (auto &b : bs) {
            for (auto &c : cs) {
                for (auto &d : ds) {
                    CHECK_SAME(jumpConditional(a.code, jumpAnd(b.code, c.code), d.code),
                        opConditional(a.code, opAnd(b.code, c.code), d.code),
                        std::string(a.name) + " ? (" + b.name + " && " + c.name + ") : " + d.name);
                }
            }
        }
    }

    // true ? (false && count) : count: nothing reads the native variable
    Natives::resetCalls();
    CHECK_EQ(run(jumpConditional(boolConstant(true), jumpAnd(boolConstant(false), native(NAT_COUNT)), native(NAT_COUNT))).text, std::string("false"));
    CHECK_EQ(count, 0);
    // true ? (true && count) : count: c is read
    CHECK_EQ(run(jumpConditional(boolConstant(true), jumpAnd(boolConstant(true), native(NAT_COUNT)), native(NAT_COUNT))).text, std::string("true"));
    CHECK_EQ(count, 1);
    // false ? (true && count) : count: d is the result
    Natives::resetCalls();
    CHECK_EQ(run(jumpConditional(boolConstant(false), jumpAnd(boolConstant(true), native(NAT_COUNT)), native(NAT_COUNT))).text, std::string("7"));
    CHECK_EQ(count, 1);
}

TEST("expressions/jumps: (a || b) ? c : d and a && (b || c)") {
    Natives natives;
    auto values = testValues();
    for (auto &a : values) {
        for (auto &b : values) {
            CHECK_SAME(jumpConditional(jumpOr(a.code, b.code), intConstant(1), intConstant(2)),
                opConditional(opOr(a.code, b.code), intConstant(1), intConstant(2)),
                std::string("(") + a.name + " || " + b.name + ") ? 1 : 2");
            CHECK_SAME(jumpAnd(a.code, jumpOr(b.code, native(NAT_COUNT))),
                opAnd(a.code, opOr(b.code, native(NAT_COUNT))),
                std::string(a.name) + " && (" + b.name + " || count)");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// assignable expressions

namespace {

bool runAssignable(const Code &code, Value &dst, const Code &end = Code{ EXPR_EVAL_INSTRUCTION_TYPE_END }) {
    harness::clearFlowRecords();
    auto bytes = assemble(code, end);
    size_t sp = g_stack.sp;
    int caseIndex = contextCase();
    bool ok = evalAssignableExpression(expr::flowState(), expr::componentIndex(caseIndex), bytes.data(), dst, FlowError::Plain("perf test"));
    CHECK_EQ((int)g_stack.sp, (int)sp);
    return ok;
}

} // namespace

TEST("expressions/jumps: an assignable t ? x : y is the variable of the taken branch") {
    Natives natives;
    Code x = caseGlobal("+g_int");
    Code y = caseGlobal("+g_double");
    REQUIRE(globalIndex(x) != globalIndex(y));
    Value *xValue = globalValue(x);
    Value *yValue = globalValue(y);

    Value dst;
    REQUIRE(runAssignable(jumpConditional(boolConstant(true), x, y), dst));
    CHECK_EQ(dst.getType(), VALUE_TYPE_VALUE_PTR);
    CHECK(dst.pValueValue == xValue);

    REQUIRE(runAssignable(jumpConditional(native(NAT_FALSE), x, y), dst));
    CHECK_EQ(dst.getType(), VALUE_TYPE_VALUE_PTR);
    CHECK(dst.pValueValue == yValue);

    // assigned like the SetVariable component does it
    assignValue(expr::flowState(), expr::componentIndex(contextCase()), dst, Value(1.25, VALUE_TYPE_DOUBLE));
    CHECK_EQ(yValue->getType(), VALUE_TYPE_DOUBLE);
    CHECK_EQ(yValue->getDouble(), 1.25);
    CHECK_EQ(xValue->getInt(), 5);

    // the END_WITH_DST_VALUE_TYPE instruction sets the type of the variable
    Code end = { EXPR_EVAL_INSTRUCTION_TYPE_END_WITH_DST_VALUE_TYPE, (uint16_t)VALUE_TYPE_INT32, 0 };
    REQUIRE(runAssignable(jumpConditional(boolConstant(true), x, y), dst, end));
    CHECK_EQ(dst.getType(), VALUE_TYPE_VALUE_PTR);
    CHECK(dst.pValueValue == xValue);
    CHECK_EQ((int)dst.dstValueType, (int)VALUE_TYPE_INT32);
}

TEST("expressions/jumps: an assignable t ? a[0] : a[1] is the element of the taken branch") {
    Natives natives;
    Code a = caseGlobal("g_int_array[0]");
    Code element = { EXPR_EVAL_INSTRUCTION_ARRAY_ELEMENT };

    Value dst;
    REQUIRE(runAssignable(jumpConditional(boolConstant(false), a + intConstant(0) + element, a + intConstant(1) + element), dst));
    REQUIRE(dst.getType() == VALUE_TYPE_ARRAY_ELEMENT_VALUE);
    CHECK_EQ(((ArrayElementValue *)dst.refValue)->elementIndex, 1);
    CHECK_EQ(dst.getValue().getInt(), 20);

    assignValue(expr::flowState(), expr::componentIndex(contextCase()), dst, Value(21, VALUE_TYPE_INT32));
    Value value;
    CHECK(expr::eval(requireCase("g_int_array[1]"), value));
    CHECK_EQ(value.getValue().getInt(), 21);
}

TEST("expressions/jumps: an assignable ?: with an error test is an error") {
    Natives natives;
    Code x = caseGlobal("+g_int");
    Code y = caseGlobal("+g_double");
    Value dst;
    CHECK(!runAssignable(jumpConditional(errorValue(), x, y), dst));
    CHECK(!runAssignable(jumpConditional(flowOutput(), x, y), dst));
    CHECK(!harness::flowErrors().empty());
}

////////////////////////////////////////////////////////////////////////////////
// other jump kinds

TEST("expressions/jumps: JUMP skips forward") {
    Natives natives;
    // 1 [JUMP 2] 2 + -> 1 (the skipped words are not evaluated)
    Code code = intConstant(1) + Code{ EXPR_EVAL_INSTRUCTION_JUMP, 2 } + intConstant(2) + op(defs_v3::OPERATION_TYPE_ADD);
    CHECK_EQ(run(code).text, std::string("1"));
    // offset 0: the next instruction
    code = intConstant(1) + Code{ EXPR_EVAL_INSTRUCTION_JUMP, 0 } + intConstant(2) + op(defs_v3::OPERATION_TYPE_ADD);
    CHECK_EQ(run(code).text, std::string("3"));
}

TEST("expressions/jumps: an unknown jump kind ends the evaluation like END") {
    Natives natives;
    // like a type 7 instruction of a newer Studio in the old evaluator
    auto bytes = assemble(intConstant(1) + Code{ (uint16_t)(EXPR_EVAL_INSTRUCTION_TYPE_JUMP | 4) } + intConstant(2));
    size_t sp = g_stack.sp;
    Value value;
    int numInstructionBytes = 0;
    CHECK(evalExpression(expr::flowState(), expr::componentIndex(contextCase()), bytes.data(), value, FlowError::Plain("perf test"), &numInstructionBytes));
    CHECK_EQ(value.getInt(), 1);
    CHECK_EQ(numInstructionBytes, 4);
    CHECK_EQ((int)g_stack.sp, (int)sp);
}

////////////////////////////////////////////////////////////////////////////////
// String.format and +: results

namespace {

std::string formatWith(const char *format, Value &result, bool &ok) {
    g_natives[NAT_STR].value = Value::makeStringRef(format, -1, 0);
    harness::clearFlowRecords();
    auto bytes = assemble(intConstant(42) + native(NAT_STR) + op(defs_v3::OPERATION_TYPE_STRING_FORMAT));
    ok = evalExpression(expr::flowState(), expr::componentIndex(contextCase()), bytes.data(), result, FlowError::Plain("perf test"));
    return ok && result.isString() ? result.getString() : "";
}

} // namespace

TEST("expressions/String.format: results that fit the stack buffer and longer ones") {
    Natives natives;
    Value result;
    bool ok;

    // 127 characters: the stack buffer, 128 and more: the heap
    const int lengths[] = { 2, 63, 64, 126, 127, 128, 129, 300, 1000 };
    for (int length : lengths) {
        char format[16];
        snprintf(format, sizeof(format), "%%%dd", length);
        std::string str = formatWith(format, result, ok);
        REQUIRE(ok);
        CHECK_EQ((int)str.size(), length);
        CHECK_EQ(str.substr(str.size() - 2), std::string("42"));
        CHECK(str.find_first_not_of(' ') == str.size() - 2);
    }

    // a %s longer than the buffer
    std::string longText(500, 'x');
    g_natives[NAT_COUNT].value = Value::makeStringRef(longText.c_str(), -1, 0);
    g_natives[NAT_STR].value = Value::makeStringRef("<%s>", -1, 0);
    auto bytes = assemble(native(NAT_COUNT) + native(NAT_STR) + op(defs_v3::OPERATION_TYPE_STRING_FORMAT));
    REQUIRE(evalExpression(expr::flowState(), expr::componentIndex(contextCase()), bytes.data(), result, FlowError::Plain("perf test")));
    CHECK_EQ(std::string(result.getString()), "<" + longText + ">");

    // the parseFormat errors stay
    formatWith("%d %d", result, ok);
    CHECK(!ok);
    formatWith("abc", result, ok);
    CHECK(!ok);
    CHECK_EQ(formatWith("100%% %d", result, ok), std::string("100% 42"));
}

////////////////////////////////////////////////////////////////////////////////
// String.format and +: allocations

TEST("expressions/perf: String.format with a short result allocates only the result string") {
    int caseIndex = requireCase("String.format(\"%d\", 42)");
    Value result;
    REQUIRE(expr::eval(caseIndex, result));
    CHECK_STR(result.getString(), "42");
    if (!canCountAllocs()) {
        printf("allocation counts need a sanitizer build\n");
        return;
    }

    int stringAllocs = countAllocs([] { Value str = Value::makeStringRef("42", 2, 0); });
    bool ok = false;
    int formatAllocs = countAllocs([&] { ok = expr::eval(caseIndex, result); });
    CHECK(ok);
    printf("makeStringRef: %d allocations, String.format(\"%%d\", 42): %d allocations\n", stringAllocs, formatAllocs);
    CHECK_EQ(formatAllocs, stringAllocs);

    caseIndex = requireCase("String.format(\"%.2f\", 3.14159)");
    formatAllocs = countAllocs([&] { ok = expr::eval(caseIndex, result); });
    CHECK(ok);
    printf("String.format(\"%%.2f\", 3.14159): %d allocations\n", formatAllocs);
    CHECK_EQ(formatAllocs, stringAllocs);
}

TEST("expressions/perf: String.format with a long result allocates a temporary buffer too") {
    Natives natives;
    Value result;
    bool ok;
    formatWith("%d", result, ok);
    if (!canCountAllocs()) {
        printf("allocation counts need a sanitizer build\n");
        return;
    }

    int stringAllocs = countAllocs([] { Value str = Value::makeStringRef("42", 2, 0); });
    auto measure = [&](const char *format) {
        g_natives[NAT_STR].value = Value::makeStringRef(format, -1, 0);
        auto bytes = assemble(intConstant(42) + native(NAT_STR) + op(defs_v3::OPERATION_TYPE_STRING_FORMAT));
        int allocs = countAllocs([&] { ok = evalExpression(expr::flowState(), expr::componentIndex(contextCase()), bytes.data(), result, FlowError::Plain("perf test")); });
        CHECK(ok);
        printf("String.format(\"%s\", 42): %d allocations\n", format, allocs);
        return allocs;
    };
    CHECK_EQ(measure("%127d"), stringAllocs);
    CHECK_EQ(measure("%128d"), stringAllocs + 1);
}

TEST("expressions/perf: a string + allocates only the concatenated string") {
    Value result;
    int caseIndex = requireCase("\"a\" + \"b\"");
    REQUIRE(expr::eval(caseIndex, result));
    CHECK_STR(result.getString(), "ab");
    if (!canCountAllocs()) {
        printf("allocation counts need a sanitizer build\n");
        return;
    }

    int concatAllocs = countAllocs([] { Value str = Value::concatenateString(Value("a"), Value("b")); });
    int stringAllocs = countAllocs([] { Value str = Value::makeStringRef("5", 1, 0); });
    bool ok = false;
    int plusAllocs = countAllocs([&] { ok = expr::eval(caseIndex, result); });
    CHECK(ok);
    printf("concatenateString: %d allocations, \"a\" + \"b\": %d allocations\n", concatAllocs, plusAllocs);
    CHECK_EQ(plusAllocs, concatAllocs);

    // a number is converted to a string value first
    caseIndex = requireCase("5 + \"x\"");
    plusAllocs = countAllocs([&] { ok = expr::eval(caseIndex, result); });
    CHECK(ok);
    CHECK_STR(result.getString(), "5x");
    printf("5 + \"x\": %d allocations\n", plusAllocs);
    CHECK_EQ(plusAllocs, stringAllocs + concatAllocs);
}

TEST("expressions/perf: the String.format of the branch that is not taken is not allocated") {
    Natives natives;
    Code format = caseCode("String.format(\"%d\", 42)");
    Value result;
    REQUIRE(run(jumpConditional(boolConstant(true), format, format)).text == "\"42\"");
    if (!canCountAllocs()) {
        printf("allocation counts need a sanitizer build\n");
        return;
    }

    int stringAllocs = countAllocs([] { Value str = Value::makeStringRef("42", 2, 0); });
    auto measure = [&](const Code &code) {
        auto bytes = assemble(code);
        bool ok = false;
        int allocs = countAllocs([&] { ok = evalExpression(expr::flowState(), expr::componentIndex(contextCase()), bytes.data(), result, FlowError::Plain("perf test")); });
        CHECK(ok);
        return allocs;
    };

    int jumpAllocs = measure(jumpConditional(boolConstant(true), format, format));
    int opAllocs = measure(opConditional(boolConstant(true), format, format));
    printf("t ? String.format(...) : String.format(...): %d allocations with jumps, %d with CONDITIONAL\n", jumpAllocs, opAllocs);
    CHECK_EQ(jumpAllocs, stringAllocs);
    CHECK_EQ(opAllocs, 2 * stringAllocs);

    // "" && String.format(...)
    jumpAllocs = measure(jumpAnd(stringConstant(""), format));
    opAllocs = measure(opAnd(stringConstant(""), format));
    printf("\"\" && String.format(...): %d allocations with jumps, %d with LOGICAL_AND\n", jumpAllocs, opAllocs);
    CHECK_EQ(jumpAllocs, 0);
    CHECK_EQ(opAllocs, stringAllocs);
}
