"""Expression test cases.

Each case is (expression, expected type, expected value[, known bug id]).

Expected types:
    int32, uint32, int64, float, double, boolean, string, undefined, null,
    date     - the value has this type and the value (float: 1e-6 relative,
               double: 1e-12 relative; nan and inf are allowed)
    number   - any numeric type with this value (1e-6 relative), for cases
               where the type is not what is checked
    prefix   - a string that starts with the value
    error    - the evaluation fails; the value is None or a text the error
               message must contain
    array    - an array; the value is its length, or a list of the expected
               elements (int, float, F(float) for a float element, str, bool,
               None for null, UNDEFINED, nested lists)
    any      - only checks that the evaluation finishes and leaves the stack
               as it was (used by the C++ tests of expressions_flow_test.cpp)

The expected value is the correct one: Studio's JavaScript definition of the
operation (project-editor/flow/expression/operations.tsx), the JS semantics of
the operators and the documented type rules (docs/draft/variables/variables.md
of Studio). When the framework gives something else today, the case names the
review finding as its known bug, so the test reports an expected failure until
the bug is fixed. Behaviour that differs from JS on purpose (or where JS and
the documentation disagree) is pinned as it is, with a comment.

The values the project can't hold (uint32 above 2^31, int64, the narrow
integer types, a float with a unit, undefined) come from the native variables
expr_nat_* (parts/expr_extra.py, expressions/expressions_native.cpp):
    expr_nat_u32   uint32 3000000000
    expr_nat_i64   int64 5000000000
    expr_nat_u8    uint8 200, i8 int8 -5, i16 int16 -300, built with the narrow
                   constructors, which leave the other bytes of the union as
                   they were (0xCC here, see C2)
    expr_nat_volt  float 2.5 V
    expr_nat_undef undefined
    expr_nat_dyn   dynamic array [10, 20, 30]

Global variables: g_* in parts/base.py, expr_* in parts/expr_extra.py.
Every case also checks that the evaluation leaves the evaluation stack as it was.
"""

import math


class _Undefined:
    def __repr__(self):
        return "UNDEFINED"


UNDEFINED = _Undefined()


class F(float):
    """An expected float (single precision) array element."""


NAN = float("nan")
INF = float("inf")

# Date.make(2024, 3, 15, 10, 20, 30, 456) in ms (time zone 0, no DST)
DT = 1710498030456
DT_ARGS = "2024, 3, 15, 10, 20, 30, 456"

CASES = [
    ############################################################################
    # literals

    ("42", "int32", 42),
    ("-7", "int32", -7),
    ("0", "int32", 0),
    ("2147483647", "int32", 2147483647),
    ("0x10", "int32", 16),
    ("0xFF", "int32", 255),
    # an integer literal without a "." is an integer, even with an exponent
    ("1e3", "int32", 1000),
    ("1.5", "double", 1.5),
    ("1.0", "double", 1.0),
    (".5", "double", 0.5),
    ("1.5e3", "double", 1500.0),
    ("2.5e-3", "double", 0.0025),
    # Studio writes an integer literal above the int32 range into an INT32
    # constant (build/values.ts), so it becomes -1294967296
    ("3000000000", "number", 3000000000, "NEW-expressions-1"),
    ('"abc"', "string", "abc"),
    ("'single'", "string", "single"),
    ('""', "string", ""),
    ('"a\\nb"', "string", "a\nb"),
    ('"tab\\there"', "string", "tab\there"),
    ('"q\\"q"', "string", 'q"q'),
    ('"back\\\\slash"', "string", "back\\slash"),
    ('"\\x41\\u00e9"', "string", "A\u00e9"),
    ('"\u00b5s"', "string", "\u00b5s"),
    ("`a${g_int}b`", "string", "a5b"),
    ("`${g_double}`", "string", "2.5"),
    ("`x`", "string", "x"),
    ("true", "boolean", True),
    ("false", "boolean", False),
    ("null", "null", None),
    ("g_bool", "boolean", True),
    ("Math.PI", "double", math.pi),
    ("Math.Infinity", "double", INF),
    ("expr_Mode.Auto", "int32", 5),
    ("expr_Mode.On + 1", "int32", 2),
    ("expr_mode", "int32", 5),
    ("expr_mode == expr_Mode.Auto", "boolean", True),
    ("expr_mode > expr_Mode.On", "boolean", True),

    ############################################################################
    # + (docs table: string wins, then double, float, integer; other types are
    # an error)

    ("1 + 2", "int32", 3),
    ("-5 + 3", "int32", -2),
    ("1 + 0.5", "double", 1.5),
    ("0.5 + 1", "double", 1.5),
    ("1.5 + 1", "double", 2.5),
    ("expr_float + 1", "float", 3.5),
    ("expr_float + 0.25", "double", 2.75),
    ("expr_float + expr_float", "float", 5.0),
    ("1 + true", "int32", 2),
    ("true + true", "int32", 2),
    ("true + 1.5", "double", 2.5),
    ("expr_nat_i64 + 1", "int64", 5000000001),
    ('"a" + "b"', "string", "ab"),
    ('"n=" + 5', "string", "n=5"),
    ('5 + "x"', "string", "5x"),
    ('"v=" + 1.5', "string", "v=1.5"),
    ('"v=" + expr_float', "string", "v=2.5"),
    ('"b=" + true', "string", "b=true"),
    ('"" + expr_nat_u32', "string", "3000000000"),
    ('"" + expr_nat_i64', "string", "5000000000"),
    ('"" + expr_nat_u8', "string", "200"),
    ('"" + expr_nat_i8', "string", "-5"),
    ('g_string + "!"', "string", "text!"),
    # the instructions of this case are reused by the Flow.translate test
    ('"expr_tr_a" + "expr_tr_b"', "string", "expr_tr_aexpr_tr_b"),
    # the docs table: string + other type is an error (JS gives "null"; O10:
    # null was converted to "" because of `!a_valid && !b_valid`)
    ('"" + null', "error", None),
    # a double in a string is formatted with %g (6 significant digits)
    ('"x" + 1234567.5', "string", "x1234567.5", "C11"),
    ('"" + Math.PI', "string", "3.141592653589793", "C11"),
    ('"" + 0.000001', "string", "0.000001", "C11"),
    ('"" + (0.1 + 0.2)', "string", "0.30000000000000004", "C11"),
    ("g_int_array + g_int_array", "error", None),
    # blobs are concatenated (JS would concatenate their texts)
    ("Array.length(Blob.allocate(2) + Blob.allocate(3))", "uint32", 5),
    ("Array.length(Blob.allocate(2) + null)", "uint32", 2),
    ("Blob.allocate(2) + 1", "error", None),
    # an int32 overflow gives the exact result as an int64 (O10: undefined
    # behaviour in op_add)
    ("2147483647 + 1", "number", 2147483648),
    # an array or undefined operand is an error (O10: `!a_valid && !b_valid`
    # added it as raw bits)
    ("1 + g_int_array", "error", None),
    ("g_int + expr_nat_undef", "error", None),
    # the operands are read with toInt64 (O10, C2: raw int32Value reads made a
    # uint32 above 2^31 negative, the narrow types had the bytes their
    # constructors didn't set)
    ("expr_nat_u32 + 1", "number", 3000000001),
    ("expr_nat_u8 + 1", "int32", 201),

    ############################################################################
    # - * /

    ("7 - 10", "int32", -3),
    ("2.5 - 1", "double", 1.5),
    ("expr_float - 1", "float", 1.5),
    ("true - 1", "int32", 0),
    ('"a" - 1', "error", None),
    # pinned: null is not a number for - (JS gives 1, the docs table an error)
    ("1 - null", "error", None),
    ("-2147483647 - 2", "number", -2147483649),
    ("expr_nat_u32 - 1", "number", 2999999999),
    ("expr_nat_i16 - 1", "int32", -301),
    ("6 * 7", "int32", 42),
    ("1.5 * 2", "double", 3.0),
    ("expr_float * 2", "float", 5.0),
    ("true * 3", "int32", 3),
    ("expr_nat_i64 * 2", "int64", 10000000000),
    ('"a" * 2', "error", None),
    ("65536 * 65536", "number", 4294967296),
    ("expr_nat_i8 * 2", "int32", -10),
    ("expr_nat_u32 * 2", "number", 6000000000),
    # integer / integer is a double
    ("7 / 2", "double", 3.5),
    ("6 / 3", "double", 2.0),
    ("1 / 4", "double", 0.25),
    ("-7 / 2", "double", -3.5),
    ("5 / 2.0", "double", 2.5),
    ("true / 2", "double", 0.5),
    ("expr_float / 2", "float", 1.25),
    ("expr_nat_i64 / 2", "double", 2500000000.0),
    ("1.0 / 0", "double", INF),
    ("-1.0 / 0", "double", -INF),
    ("0.0 / 0", "double", NAN),
    ("expr_float / 0", "float", INF),
    # pinned: integer division by zero is an error (JS gives Infinity)
    ("1 / 0", "error", None),
    ("expr_nat_i64 / 0", "error", None),
    # pinned: a numeric string is not a number (JS converts it)
    ('"6" / 2', "error", None),
    ("expr_nat_u32 / 1000", "double", 3000000.0),

    ############################################################################
    # % (JS: the sign of the dividend)

    ("7 % 3", "int32", 1),
    ("-7 % 3", "int32", -1),
    ("7 % -3", "int32", 1),
    ("true % 2", "int32", 1),
    ("7.5 % 2", "double", 1.5),
    ("expr_float % 2", "float", 0.5),
    ("expr_nat_i64 % 7", "int64", 5000000000 % 7),
    ("7.0 % 0", "double", NAN),
    # pinned: integer modulo by zero is an error (JS gives NaN)
    ("7 % 0", "error", None),
    ('"a" % 2', "error", None),
    # the double and float modulo has the sign of the dividend (O10: floor gave
    # 2 for -7.0 % 3)
    ("-7.0 % 3", "double", -1.0),
    ("-expr_float % 2", "float", -0.5),
    # x % -1 is 0 (O10: INT_MIN % -1 trapped)
    ("(-2147483647 - 1) % -1", "number", 0),
    ("expr_nat_u32 % 7", "number", 3000000000 % 7),

    ############################################################################
    # shifts and bitwise operators

    ("1 << 4", "int32", 16),
    ("-16 >> 2", "int32", -4),
    ("1 << 31", "int32", -2147483648),
    ("5 & 3", "int32", 1),
    ("5 | 3", "int32", 7),
    ("5 ^ 3", "int32", 6),
    ("-1 & 255", "int32", 255),
    ("5 ^ -1", "int32", -6),
    ("true | 2", "int32", 3),
    ("expr_nat_u32 >> 1", "int32", -647483648),
    # int64 operands keep 64 bits (JS has no int64)
    ("expr_nat_i64 >> 32", "int64", 1),
    ("expr_nat_i64 & 255", "int64", 0),
    # pinned: bitwise operators need integers (JS converts 1.5 to 1)
    ("1.5 & 1", "error", None),
    ("~1.5", "error", None),
    # the shift count is masked to 5 bits like in JS (O10: a count of 32 or
    # more was undefined behaviour)
    ("1 << 32", "int32", 1),
    ("16 >> 33", "int32", 8),
    # ~ of an int32 is an int32, of an int64 an int64 (O11: it returned an
    # unsigned type)
    ("~5", "int32", -6),
    ("~0", "int32", -1),
    ("~-1", "int32", 0),
    ("~5 < 0", "boolean", True),
    ("~expr_nat_i64", "int64", -5000000001),
    # the int32 result of ~ in integer arithmetic
    ("(~5) + 0", "int32", -6),

    ############################################################################
    # comparison

    ("1 < 2", "boolean", True),
    ("2 <= 1", "boolean", False),
    ("2 <= 2", "boolean", True),
    ("3 > 2", "boolean", True),
    ("2 > 2", "boolean", False),
    ("2 >= 2", "boolean", True),
    ("1 >= 2", "boolean", False),
    ("1 == 1", "boolean", True),
    ("1 != 1", "boolean", False),
    ("1 == 2", "boolean", False),
    ("1 == 1.0", "boolean", True),
    ("1 < 1.5", "boolean", True),
    ("1.5 <= 1.5", "boolean", True),
    ("1 == true", "boolean", True),
    ("2 == true", "boolean", False),
    ("true > false", "boolean", True),
    ('"1" == 1', "boolean", True),
    ('"" == 0', "boolean", True),
    ('"abc" == 0', "boolean", False),
    ('"a" == "a"', "boolean", True),
    ('"abc" == "abd"', "boolean", False),
    ('"abc" != "abd"', "boolean", True),
    ('"a" < "b"', "boolean", True),
    ('"b" < "a"', "boolean", False),
    ('"B" < "a"', "boolean", True),
    ('"b" > "a"', "boolean", True),
    ('"a" >= "a"', "boolean", True),
    ('"10" < "9"', "boolean", True),
    ('"10" > 9', "boolean", True),
    ('"abc" < 5', "boolean", False),
    ('g_string == "text"', "boolean", True),
    ("null == null", "boolean", True),
    ("expr_nat_undef == null", "boolean", True),
    ("null == 0", "boolean", False),
    ("null == false", "boolean", False),
    ('"a" == null', "boolean", False),
    ("g_int_array == g_int_array", "boolean", True),
    ("[1] == [1]", "boolean", False),
    # pinned: blobs compare by content (JS compares the objects)
    ("Blob.allocate(2) == Blob.allocate(2)", "boolean", True),
    ("expr_nat_u32 > 2147483647", "boolean", True),
    ("expr_nat_i64 > 4294967296.0", "boolean", True),
    ("expr_nat_i64 == 5000000000.0", "boolean", True),
    ("expr_nat_u8 == 200", "boolean", True),
    ("expr_nat_i8 < 0", "boolean", True),
    ("expr_nat_volt == 2.5", "boolean", True),
    ("expr_float == 2.5", "boolean", True),
    ("0.0 / 0 < 1", "boolean", False),
    ("0.0 / 0 == 0.0 / 0", "boolean", False),
    ("Date.make(2024, 1, 1, 0, 0, 0, 0) < Date.make(2024, 1, 2, 0, 0, 0, 0)", "boolean", True),
    # pinned: dates compare by value (JS compares the Date objects)
    ("Date.make(2024, 1, 1, 0, 0, 0, 0) == Date.make(2024, 1, 1, 0, 0, 0, 0)", "boolean", True),
    # is_equal compares a uint32 and an int32 with toInt32: 3000000000 equals
    # -1294967296
    ("expr_nat_u32 == -1294967296", "boolean", False, "NEW-expressions-3"),
    # == of two floats or two doubles also compares the unit: constants and
    # variables have UNIT_NONE, computed values UNIT_UNKNOWN
    ("expr_nat_volt == expr_float", "boolean", True, "O11"),
    ('expr_float == Flow.parseFloat("2.5")', "boolean", True, "O11"),
    ("0.5 + 1 == 1.5", "boolean", True, "O11"),
    ("0.5 + 1 != 1.5", "boolean", False, "O11"),
    ("2.5 <= 5.0 / 2", "boolean", True, "O11"),
    ("5.0 / 2 > 2.5", "boolean", False, "O11"),
    ("g_double * 1 == g_double", "boolean", True, "O11"),
    # > is !< && !=, >= is !<: NaN, undefined and non-numeric strings are
    # greater than anything
    ("0.0 / 0 > 1", "boolean", False, "O11"),
    ("0.0 / 0 >= 5", "boolean", False, "O11"),
    ("expr_nat_undef > 0", "boolean", False, "O11"),
    ("expr_nat_undef >= 0", "boolean", False, "O11"),
    ('"abc" > 5', "boolean", False, "O11"),

    ############################################################################
    # logical operators and ! (the result is a boolean, as Studio types it;
    # JS && and || would return one of the operands)

    ("true && false", "boolean", False),
    ("true && true", "boolean", True),
    ("true || false", "boolean", True),
    ("false || false", "boolean", False),
    ("1 && 2", "boolean", True),
    ('0 || "x"', "boolean", True),
    ('"" && true', "boolean", False),
    ("g_int_array && true", "boolean", True),
    # pinned: an empty array is false (in JS it is truthy)
    ("expr_empty_array || false", "boolean", False),
    ("null || false", "boolean", False),
    # the side that isn't needed is not evaluated (Studio compiles && and ||
    # with jumps, O16), so its error doesn't matter
    ("false && g_int_array[5]", "boolean", False),
    ("true || g_int_array[5]", "boolean", True),
    ("true && g_int_array[5]", "error", None),
    ("g_int_array[5] || true", "error", None),
    ("!true", "boolean", False),
    ("!0", "boolean", True),
    ("!0.0", "boolean", True),
    ('!""', "boolean", True),
    ('!"abc"', "boolean", False),
    ("!null", "boolean", True),
    ("!expr_nat_undef", "boolean", True),
    ("!g_int_array", "boolean", False),
    ("!g_int_array[5]", "error", None),
    # NaN is true in toBool (JS: falsy)
    ("0.0 / 0 || false", "boolean", False, "NEW-expressions-2"),
    ("!(0.0 / 0)", "boolean", True, "NEW-expressions-2"),

    ############################################################################
    # unary + and -

    ("-g_int", "int32", -5),
    ("-g_double", "double", -2.5),
    ("-expr_float", "float", -2.5),
    ("+g_int", "int32", 5),
    ("+g_double", "double", 2.5),
    ("+expr_float", "float", 2.5),
    ("-expr_nat_i64", "int64", -5000000000),
    # pinned: unary + and - need a number (JS converts strings and booleans)
    ('-"5"', "error", None),
    ("-true", "error", None),
    # unary - and + read the value with toInt64 (O10: the uint32 and uint8
    # paths read the signed members)
    ("-expr_nat_u32", "number", -3000000000),
    ("-expr_nat_u8", "number", -200),
    ("+expr_nat_u8", "number", 200),
    ("-(-2147483647 - 1)", "number", 2147483648),

    ############################################################################
    # conditional operator

    ('1 < 2 ? "yes" : "no"', "string", "yes"),
    ("false ? 1 : 2", "int32", 2),
    ('0 ? "a" : "b"', "string", "b"),
    ('"" ? 1 : 2', "int32", 2),
    ('"0" ? 1 : 2', "int32", 1),
    ("null ? 1 : 2", "int32", 2),
    ("g_int_array ? 1 : 2", "int32", 1),
    ('g_bool ? 1 : "a"', "int32", 1),
    ('!g_bool ? 1 : "a"', "string", "a"),
    ('g_int > 3 ? (g_int > 4 ? "a" : "b") : "c"', "string", "a"),
    ("true ? 1 : g_int_array[5]", "int32", 1),
    ("false ? g_int_array[5] : 3", "int32", 3),
    ("true ? g_int_array[5] : 3", "error", None),
    ("g_int_array[5] ? 1 : 2", "error", None),
    ("0.0 / 0 ? 1 : 2", "int32", 2, "NEW-expressions-2"),
    # an error inside Math.min must not leave an operand on the stack, which
    # the conditional would take as its condition (O1)
    ("g_int < 0 ? Math.min(g_int_array[5], 1) : 0", "int32", 0),
    # ?:, && and || inside other expressions (jumps, O16)
    ("g_bool ? (g_int > 3 && g_int < 10) : false", "boolean", True),
    ('(g_int > 3 || g_bool) ? "a" : "b"', "string", "a"),
    ("(false ? 1 : 2) + 1", "int32", 3),
    ('String.format("%d", g_bool ? 1 : 2)', "string", "1"),
    ("g_bool && (g_int == 5 || g_int_array[5])", "boolean", True),
    ("false || (g_int_array[5] && true)", "error", None),
    ("!g_bool ? g_int_array[5] : g_int", "int32", 5),

    ############################################################################
    # array elements and struct fields

    ("g_int_array[0]", "int32", 10),
    ("g_int_array[1]", "int32", 20),
    ("g_int_array[2]", "int32", 30),
    ('g_int_array["1"]', "int32", 20),
    # pinned: the index is converted with toInt32 (JS gives undefined)
    ("g_int_array[1.7]", "int32", 20),
    ("g_int_array[true]", "int32", 20),
    # pinned: an index out of range is an error (JS gives undefined)
    ("g_int_array[5]", "error", None),
    ("g_int_array[-1]", "error", None),
    ("[1, 2, 3][g_int - 4]", "int32", 2),
    ("expr_str_array[2]", "string", "c"),
    ("expr_dbl_array[1]", "double", 1.5),
    ("expr_matrix[1][0]", "int32", 3),
    ("expr_matrix[1]", "array", [3, 4]),
    ("expr_point.x", "int32", 1),
    ("expr_point.y", "int32", 2),
    ("expr_rect.name", "string", "r"),
    ("expr_rect.p.y", "int32", 4),
    ("expr_rect.tags[1]", "string", "t2"),
    ("expr_rect.w", "double", 1.5),
    ("expr_points[1].y", "int32", 6),
    ("expr_points[2].x", "error", None),
    ("expr_nat_dyn[1]", "int32", 20),
    ("expr_nat_dyn[3]", "error", None),
    ("Array.allocate(2)[0]", "undefined", None),
    ("Blob.allocate(4)[0]", "uint32", 0),
    ('Crypto.sha256("abc")[0]', "uint32", 0xBA),
    ('Crypto.sha256("abc")[31]', "uint32", 0xAD),
    ('Flow.makeValue("struct:expr_Point", {x: 3, y: 4}).y', "int32", 4),
    ('Flow.makeValue("struct:expr_Point", {y: 4}).x', "undefined", None),
    ('Flow.makeValue("struct:expr_Rect", {name: "n", p: Flow.makeValue("struct:expr_Point", {x: 7, y: 8}), tags: ["u", "v"], w: 2.0}).p.y',
     "int32", 8),
    ('Flow.makeValue("struct:expr_Rect", {name: "n", tags: ["u", "v"]}).tags[1]', "string", "v"),
    ('[Flow.makeValue("struct:expr_Point", {x: 1, y: 2})][0].y', "int32", 2),
    ("expr_points[0]", "array", [1, 2]),
    ('String.split("a,b", ",")[1]', "string", "b"),
    # the error keeps its description (F8: it was lost)
    ("g_int_array[3]", "error", "Array element index out of bounds"),
    ('g_int_array["x"]', "error", "Integer value expected for array element index"),
    ("Blob.allocate(2)[2]", "error", "Blob element index out of bounds"),

    ############################################################################
    # array literals

    ("[1, 2, 3]", "array", [1, 2, 3]),
    ("[]", "array", []),
    ('[g_int, g_double, "s", true, null]', "array", [5, 2.5, "s", True, None]),
    ("[[1, 2], [3]]", "array", [[1, 2], [3]]),
    # 17 and 18 elements and the 3 values of Flow.makeArrayValue (the tests
    # configuration has an evaluation stack of 64 entries, F8)
    ("[" + ", ".join(str(i) for i in range(1, 18)) + "]", "array", list(range(1, 18))),
    ("[" + ", ".join(str(i) for i in range(1, 19)) + "]", "array", list(range(1, 19))),
    # an element that is an error is stored in the array
    ("[g_int_array[5]]", "error", None, "O13"),

    ############################################################################
    # Array.*, Struct.*

    ("Array.length(g_int_array)", "uint32", 3),
    ("Array.length([])", "uint32", 0),
    ("Array.length(expr_point)", "uint32", 2),
    ("Array.length(Blob.allocate(5))", "uint32", 5),
    ("Array.length(expr_nat_dyn)", "uint32", 3),
    # the uint32 length in integer arithmetic and comparisons
    ("Array.length(expr_str_array) + 1", "int32", 4),
    ("Array.length(g_int_array) - 4", "int32", -1),
    ("Array.length(g_int_array) > -1", "boolean", True),
    # pinned: not for strings (JS gives 3)
    ('Array.length("abc")', "error", None),
    ("Array.slice(g_int_array, 1)", "array", [20, 30]),
    ("Array.slice(g_int_array, 0, 2)", "array", [10, 20]),
    ("Array.slice(g_int_array)", "array", [10, 20, 30]),
    ("Array.slice(g_int_array, 3)", "array", []),
    ("Array.slice(5, 1)", "error", None),
    ("Array.slice(g_int_array, 1, 10)", "array", [20, 30], "O13"),
    ("Array.slice(g_int_array, 5)", "array", [], "O13"),
    ("Array.slice(g_int_array, -1)", "array", [30], "O13"),
    ("Array.slice(g_int_array, 0, -1)", "array", [10, 20], "O13"),
    ("Array.slice(g_int_array, 2, 1)", "array", [], "O13"),
    ("Array.slice(g_int_array, g_int_array[9], 2)", "error", None),
    ("Array.slice(g_int_array[9], 1)", "error", None),
    ("Array.length(Array.allocate(3))", "uint32", 3),
    ("Array.allocate(0)", "array", []),
    # a negative size is an error (O7: it was stored as 4294967295 elements in
    # a 1-element array)
    ("Array.allocate(-1)", "error", None),
    ("Array.append(g_int_array, 40)", "array", [10, 20, 30, 40]),
    ('Array.append([], "x")', "array", ["x"]),
    ("Array.append(5, 1)", "error", None),
    ("Array.append(g_int_array, g_int_array[9])", "error", None),
    ("Array.append(g_int_array[9], 1)", "error", None),
    ("Array.insert(g_int_array, 1, 15)", "array", [10, 15, 20, 30]),
    ("Array.insert(g_int_array, 0, 5)", "array", [5, 10, 20, 30]),
    ("Array.insert(g_int_array, 10, 40)", "array", [10, 20, 30, 40]),
    # pinned: a negative position is clamped to 0 (a JS slice based insert
    # would count it from the end)
    ("Array.insert(g_int_array, -1, 5)", "array", [5, 10, 20, 30]),
    ("Array.insert(g_int_array[9], 0, 1)", "error", None),
    ("Array.remove(g_int_array, 1)", "array", [10, 30]),
    ("Array.remove(g_int_array, 0)", "array", [20, 30]),
    # pinned: a position out of range is an error
    ("Array.remove(g_int_array, 3)", "error", None),
    ("Array.remove(g_int_array[9], 1)", "error", None),
    ("Array.clone(g_int_array)", "array", [10, 20, 30]),
    ("Array.clone(expr_matrix)", "array", [[1, 2], [3, 4]]),
    ("Array.clone(expr_rect).p.x", "int32", 3),
    ("Array.clone(5)", "int32", 5),
    ('Array.clone("abc")', "string", "abc"),
    ("Array.isEqual(g_int_array, [10, 20, 30])", "boolean", True),
    ("Array.isEqual(g_int_array, [10, 20])", "boolean", False),
    ("Array.isEqual([1, [2, 3]], [1, [2, 3]])", "boolean", True),
    ("Array.isEqual([1, [2, 3]], [1, [2, 4]])", "boolean", False),
    ("Array.isEqual([1.0], [1])", "boolean", True),
    ("Array.isEqual(g_int_array[9], [1])", "error", None),
    # the elements are compared with is_equal, which compares the units (O11)
    ("Array.isEqual([0.5], [1.0 / 2])", "boolean", True, "O11"),
    ('Struct.isEqual(expr_point, Flow.makeValue("struct:expr_Point", {x: 1, y: 2}))', "boolean", True),
    ("Struct.clone(expr_point).y", "int32", 2),
    ('Flow.makeValue("struct:expr_Point", {x: 3, y: 4})', "array", [3, 4]),

    ############################################################################
    # String.*

    ('String.length("hello")', "int32", 5),
    ('String.length("")', "int32", 0),
    ("String.length(g_string)", "int32", 4),
    # pinned: not for numbers
    ("String.length(5)", "error", None),
    # byte offsets: JS counts UTF-16 code units
    ('String.length("\u00b5s")', "int32", 2, "O13"),
    ('String.substring("hello", 1, 3)', "string", "el"),
    ('String.substring("hello", 1)', "string", "ello"),
    ('String.substring("hello", 2, 99)', "string", "llo"),
    ('String.substring("hello", -2, 2)', "string", "he"),
    ('String.substring("hello", 1.9, 3)', "string", "el"),
    ("String.substring(5, 1)", "error", None),
    ('String.substring("hello", 1, g_int_array[9])', "error", None),
    # JS swaps start > end
    ('String.substring("hello", 3, 1)', "string", "el", "O13"),
    ('String.substring("\u00b5s", 1)', "string", "s", "O13"),
    ('String.substring("hello", g_int_array[9], 3)', "error", None),
    ('String.find("hello", "ll")', "int32", 2),
    ('String.find("hello", "z")', "int32", -1),
    ('String.find("hello", "")', "int32", 0),
    # pinned: the arguments are converted to strings (JS has no indexOf on numbers)
    ("String.find(12345, 3)", "int32", 2),
    ('String.find("a\u00b5b", "b")', "int32", 2, "O13"),
    ('String.find(g_int_array[9], "a")', "error", None),
    ('String.padStart("5", 3, "0")', "string", "005"),
    ('String.padStart("abc", 2, "0")', "string", "abc"),
    ('String.padStart("5", 5, "ab")', "string", "abab5"),
    ('String.padStart(5, 3, "0")', "string", "005"),
    ('String.padStart("5", -1, "0")', "string", "5"),
    ('String.padStart("", 3, "ab")', "string", "aba"),
    ('String.padStart("5", 3, "")', "string", "5"),
    ('String.padStart("\u00b5", 3, "0")', "string", "00\u00b5", "O13"),
    ('String.padStart(g_int_array[9], 3, "0")', "error", None),
    ('String.split("a,b,c", ",")', "array", ["a", "b", "c"]),
    ('String.split("abc", ",")', "array", ["abc"]),
    # pinned: the separator must be a string (JS converts it)
    ('String.split("a b", 5)', "error", None),
    # strtok: empty fields are dropped, the separator is a set of characters
    ('String.split("a,,b", ",")', "array", ["a", "", "b"], "O13"),
    ('String.split("a-b--c", "--")', "array", ["a-b", "c"], "O13"),
    ('String.split("abc", "")', "array", ["a", "b", "c"], "O13"),
    ('String.split("", ",")', "array", [""], "O13"),
    ('String.split(g_int_array[9], ",")', "error", None),
    ("String.fromCodePoint(65)", "string", "A"),
    ("String.fromCodePoint(181)", "string", "\u00b5"),
    ("String.fromCodePoint(8364)", "string", "\u20ac"),
    ("String.fromCodePoint(128512)", "string", "\U0001F600"),
    ('String.fromCodePoint("66")', "string", "B"),
    # JS: RangeError; here the bits of -1 are encoded as a 4-byte sequence
    ("String.fromCodePoint(-1)", "error", None, "NEW-expressions-9"),
    ('String.codePointAt("ABC", 1)', "int32", 66),
    ('String.codePointAt("a\u00b5b", 1)', "int32", 181),
    ('String.codePointAt("a\u00b5b", 2)', "int32", 98),
    ('String.codePointAt("\U0001F600", 0)', "int32", 128512),
    # pinned: past the end it is 0 (JS gives undefined)
    ('String.codePointAt("abc", 5)', "int32", 0),
    ("String.codePointAt(g_int_array[9], 0)", "error", None),

    ############################################################################
    # String.format (printf conversions; Studio's JS uses d3-format, which
    # agrees for these)

    ('String.format("%d", 42)', "string", "42"),
    ('String.format("%5d", 42)', "string", "   42"),
    ('String.format("%05d", 42)', "string", "00042"),
    ('String.format("%d", -7)', "string", "-7"),
    ('String.format("%x", 255)', "string", "ff"),
    ('String.format("%X", 255)', "string", "FF"),
    ('String.format("%.2f", 3.14159)', "string", "3.14"),
    ('String.format("%.1f", g_double)', "string", "2.5"),
    ('String.format("%.3f", expr_float)', "string", "2.500"),
    ('String.format("%.2f", 5)', "string", "5.00"),
    ('String.format("%s", "abc")', "string", "abc"),
    ('String.format("", 1)', "error", None),
    ('String.format("%q", 1)', "error", None),
    ("String.format(5, 1)", "error", None),
    ('String.format("%d", g_int_array[9])', "error", None),
    # integer conversions convert the value (O2: they read its raw bits)
    ('String.format("%02d", 2.0)', "string", "02"),
    # the MIO168+ running time display: Math.floor keeps the float type
    ('String.format("%d", Math.floor(expr_float))', "string", "2"),
    ('String.format("%d", "42")', "string", "42"),
    # %ld converts an int32 to long (O2)
    ('String.format("%ld", expr_Mode.Neg)', "string", "-7"),
    # a format with two conversions is an error (O2: it read a second vararg
    # that was never passed)
    ('String.format("%s %s", "a")', "error", None),
    ("String.format(g_int_array[9], 1)", "error", None),

    ############################################################################
    # Math.*

    ("Math.sin(0.0)", "double", 0.0),
    ("Math.sin(Math.PI / 2)", "double", 1.0),
    ("Math.cos(0.0)", "double", 1.0),
    ("Math.cos(Math.PI)", "double", -1.0),
    ("Math.sin(expr_float)", "float", math.sin(2.5)),
    ("Math.cos(expr_float)", "float", math.cos(2.5)),
    ("Math.log(10.0)", "double", math.log(10)),
    ("Math.log(0.0)", "double", -INF),
    ("Math.log(-1.0)", "double", NAN),
    ("Math.log10(1000.0)", "double", 3.0),
    ("Math.log(expr_float)", "float", math.log(2.5)),
    ("Math.log10(expr_float)", "float", math.log10(2.5)),
    # pinned: not for strings (JS converts)
    ('Math.sin("1")', "error", None),
    # an integer argument gives a float (single precision), Studio types the
    # result as double
    ("Math.sin(1)", "double", math.sin(1), "NEW-expressions-5"),
    ("Math.log10(100)", "double", 2.0, "NEW-expressions-5"),
    # the int64 path gives a double (O10: it stored a double into a FLOAT value)
    ("Math.sin(expr_nat_i64)", "number", math.sin(5e9)),
    ("Math.log10(expr_nat_u32)", "number", math.log10(3e9)),
    ("Math.pow(2, 10)", "double", 1024.0),
    ("Math.pow(2.0, 0.5)", "double", math.sqrt(2)),
    # float base and integer exponent: float here, double in Studio's types
    ("Math.pow(expr_float, 2)", "number", 6.25),
    ("Math.pow(expr_float, expr_float)", "float", 2.5 ** 2.5),
    ("Math.pow(-8.0, 1.0 / 3)", "double", NAN),
    ("Math.pow(2, g_int_array[9])", "error", None),
    ("Math.pow(expr_nat_i64, 2)", "double", 2.5e19, "O15"),
    ("Math.pow(g_int_array[9], 2)", "error", None),
    ("Math.abs(-3)", "int32", 3),
    ("Math.abs(-2.5)", "double", 2.5),
    ("Math.abs(-expr_float)", "float", 2.5),
    ("Math.abs(-expr_nat_i64)", "int64", 5000000000),
    ("Math.abs(expr_nat_i8)", "number", 5),
    ('Math.abs("-3")', "error", None),
    ("Math.abs(-2147483647 - 1)", "number", 2147483648),
    ("Math.floor(2.7)", "double", 2.0),
    ("Math.floor(-2.5)", "double", -3.0),
    ("Math.ceil(2.1)", "double", 3.0),
    ("Math.ceil(-2.5)", "double", -2.0),
    # pinned: an integer stays an integer (Studio types the result as double)
    ("Math.floor(5)", "int32", 5),
    ("Math.floor(expr_float)", "float", 2.0),
    ("Math.ceil(expr_float)", "float", 3.0),
    ('Math.floor("2.5")', "error", None),
    ("Math.floor(expr_nat_i64)", "int64", 5000000000, "O15"),
    ("Math.ceil(expr_nat_i64)", "int64", 5000000000, "O15"),
    ("Math.round(2.5)", "double", 3.0),
    ("Math.round(2.4)", "double", 2.0),
    ("Math.round(2.346, 2)", "double", 2.35),
    ("Math.round(expr_float)", "float", 3.0),
    ("Math.round(5)", "int32", 5),
    # C round() rounds half away from zero, JS Math.round half up
    ("Math.round(-2.5)", "double", -2.0, "NEW-expressions-4"),
    # roundN(double) uses a float power of ten
    ("Math.round(0.123456789012, 11)", "double", 0.12345678901, "O15"),
    # the digits argument is not checked for an error
    ("Math.round(2.5, g_int_array[9])", "error", None, "O15"),
    ("Math.round(expr_nat_i64)", "int64", 5000000000, "O15"),
    ("Math.round(g_int_array[9], 2)", "error", None),
    ("Math.min(3, 1, 2)", "int32", 1),
    ("Math.max(3, 1, 2)", "int32", 3),
    ("Math.min(1.5, 2)", "double", 1.5),
    ("Math.min(2, 1.5)", "double", 1.5),
    ("Math.max(1, 2.5)", "double", 2.5),
    ("Math.max(1, 2, 3, 4, 5, 6, 7, 8)", "int32", 8),
    ("Math.max(expr_nat_u32, 1)", "uint32", 3000000000),
    ("Math.min(expr_nat_u32, 1)", "int32", 1),
    ("Math.max(1, 0.0 / 0)", "double", NAN),
    ("Math.min(0.0 / 0, 1)", "double", NAN),
    ("Math.max(1, g_int_array[9])", "error", None),
    ("Math.max(0.0 / 0, 1)", "double", NAN, "O11"),
    ("Math.min(1, 0.0 / 0)", "double", NAN, "NEW-expressions-8"),
    ("Math.min(g_int_array[5], 1) + 1", "error", None),

    ############################################################################
    # Date.* (time zone 0, no DST; Date.getMonth is 1 to 12 and Date.getDay is
    # the day of the month, as Studio's documentation says; its JS eval uses
    # getMonth() and getDay() of JS, which give 0 to 11 and the day of the week)

    ("Date.make(%s)" % DT_ARGS, "date", DT),
    ("Date.make(1970, 1, 1, 0, 0, 0, 0)", "date", 0),
    ("Date.getYear(Date.make(%s))" % DT_ARGS, "int32", 2024),
    ("Date.getMonth(Date.make(%s))" % DT_ARGS, "int32", 3),
    ("Date.getDay(Date.make(%s))" % DT_ARGS, "int32", 15),
    ("Date.getHours(Date.make(%s))" % DT_ARGS, "int32", 10),
    ("Date.getMinutes(Date.make(%s))" % DT_ARGS, "int32", 20),
    ("Date.getSeconds(Date.make(%s))" % DT_ARGS, "int32", 30),
    ("Date.getMilliseconds(Date.make(%s))" % DT_ARGS, "int32", 456),
    ("Date.getDay(Date.make(2024, 2, 29, 0, 0, 0, 0))", "int32", 29),
    ("Date.getMonth(Date.make(2023, 2, 29, 0, 0, 0, 0))", "int32", 3),
    ("Date.getDay(Date.make(2100, 3, 1, 0, 0, 0, 0))", "int32", 1),
    ("Date.getSeconds(Date.make(2024, 1, 1, 0, 0, 0, 1500))", "int32", 1),
    # pinned: the locale format is FORMAT_DMY_24
    ("Date.toLocaleString(Date.make(%s))" % DT_ARGS, "string", "15-03-2024 10:20:30.456"),
    ('"" + Date.make(%s)' % DT_ARGS, "string", "15-03-2024 10:20:30.456"),
    ('Date.fromString("2024-03-15T10:20:30.456")', "date", DT),
    ('Date.getHours(Date.fromString("2024-03-15T10:20:30.456Z"))', "int32", 10),
    ("Date.getYear(5)", "error", None),
    ('Date.toString("x")', "error", None),
    # milliseconds are printed with 3 digits (O8)
    ("Date.toString(Date.make(%s))" % DT_ARGS, "prefix", "2024-03-15T10:20:30.456"),
    # JS normalizes the month
    ("Date.getYear(Date.make(2024, 13, 1, 0, 0, 0, 0))", "int32", 2025),
    # (O8: month 14 read monthDays[12])
    ("Date.getMonth(Date.make(2024, 14, 1, 0, 0, 0, 0))", "int32", 2),
    ("Date.getYear(Date.make(2024, 0, 1, 0, 0, 0, 0))", "int32", 2023),
    # pre-1970 (O8: unsigned arithmetic, breakDate never ended)
    ("Date.getYear(Date.make(1960, 1, 1, 0, 0, 0, 0))", "int32", 1960),
    # after 2225 (F20: breakDate kept the year offset in a uint8_t)
    ("Date.getYear(Date.make(2300, 1, 1, 0, 0, 0, 0))", "int32", 2300),
    # the fraction of a second is read as an integer number of milliseconds
    ('Date.getMilliseconds(Date.fromString("2024-03-15T10:20:30.5"))', "int32", 500, "NEW-expressions-6"),
    ("Date.make(g_int_array[9], 1, 1, 0, 0, 0, 0)", "error", None),
    # expressions_flow_test.cpp sets getDateNowHook
    ("Date.now()", "any", None),
    ("Date.getHours(Date.now())", "any", None),

    ############################################################################
    # System.*, Flow.*, Crypto.*, Blob.*

    # expressions_flow_test.cpp checks the values
    ("System.getTick()", "any", None),
    ("System.getTick() / 1000", "any", None),
    ('Flow.getBitmapIndex("expr_dot")', "any", None),
    # without iterators Flow.index is an error, and its argument is popped (O1)
    ("Flow.index(0)", "error", None),
    ("Flow.index(1)", "error", None),
    ("Flow.index(5)", "error", None),
    ("$index", "error", None),
    ("Flow.isPageActive()", "boolean", False),
    ("Flow.pageTimelinePosition()", "float", 0.0),
    ('Flow.makeArrayValue("array:integer", 3)', "array", [UNDEFINED, UNDEFINED, UNDEFINED]),
    ('Flow.makeArrayValue("array:integer", 0)', "array", []),
    ('Flow.makeArrayValue("array:integer", -1)', "error", None),
    ("Flow.languages()", "array", ["en", "de"]),
    ("Flow.themes()", "array", ["Default"]),
    ('Flow.translate("expr_hello")', "string", "Hello"),
    ('T"expr_bye"', "string", "Bye"),
    ('Flow.parseInteger("42")', "int32", 42),
    ('Flow.parseInteger("-17")', "int32", -17),
    ('Flow.parseInteger(" 42 ")', "int32", 42),
    ("Flow.parseInteger(3.7)", "int32", 3),
    ('Flow.parseInteger("abc")', "error", None),
    # pinned: the whole string must be a number (JS parseInt gives 42)
    ('Flow.parseInteger("42abc")', "error", None),
    # JS parseInt and parseFloat give NaN for ""
    ('Flow.parseInteger("")', "error", None, "NEW-expressions-7"),
    ('Flow.parseInteger("3000000000")', "error", None, "C10"),
    ('Flow.parseFloat("2.5")', "float", 2.5),
    ('Flow.parseFloat("1e3")', "float", 1000.0),
    ('Flow.parseFloat("x")', "error", None),
    ('Flow.parseDouble("0.1")', "double", 0.1),
    ('Flow.parseDouble(" -1.5e-3 ")', "double", -0.0015),
    ('Flow.parseDouble("")', "error", None, "NEW-expressions-7"),
    ("Flow.toInteger(2.9)", "int32", 2),
    ("Flow.toInteger(-2.9)", "int32", -2),
    ('Flow.toInteger("12")', "int32", 12),
    ("Flow.toInteger(true)", "int32", 1),
    # pinned: int64 is truncated to 32 bits
    ("Flow.toInteger(expr_nat_i64)", "int32", 705032704),
    ('Flow.getBitmapIndex("nope")', "int32", 0),
    ('Array.length(Crypto.sha256("abc"))', "uint32", 32),
    ("Crypto.sha256(Blob.allocate(1))[0]", "uint32", 0x6E),
    # sha256.c shifts the 0x80 padding byte by 24 in an int (undefined behaviour)
    ('Crypto.sha256("")[0]', "uint32", 0xE3, "NEW-expressions-10@san"),
    ("Crypto.sha256(5)", "error", None),
    ("Array.length(Blob.allocate(10))", "uint32", 10),
]
