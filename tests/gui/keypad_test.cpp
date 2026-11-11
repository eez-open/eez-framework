/*
 * eez-framework tests: gui/keypad.cpp, the numeric and the text keypad, on
 * the keypad pages of the gui part (keys tapped through the touch pipeline).
 */

#include <math.h>
#include <string.h>
#include <string>

#include <eez/core/util.h>

#include "gui_test.h"

using namespace gui_test;

namespace eez {
namespace gui {
// gui/keypad.cpp
void data_keypad_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_option1_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_option1_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_option2_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_option3_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_sign_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_dot_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_ok_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_can_set_default(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_edit_unit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
}
}

static Value data(void (*f)(DataOperationEnum, const WidgetCursor &, Value &), DataOperationEnum operation = DATA_OPERATION_GET) {
    Value value;
    f(operation, WidgetCursor(), value);
    return value;
}

////////////////////////////////////////////////////////////////////////////////
// numeric keypad

static NumericKeypad g_numericKeypad;
static int g_okCount;
static float g_okValue;
static uint32_t g_okUint32Value;
static int g_cancelCount;

static void okFloat(float value) {
    g_okCount++;
    g_okValue = value;
    app()->popPage();
}

static void okUint32(uint32_t value) {
    g_okCount++;
    g_okUint32Value = value;
    app()->popPage();
}

static void cancelCallback() {
    g_cancelCount++;
    app()->popPage();
}

static NumericKeypadOptions voltOptions() {
    NumericKeypadOptions options;
    options.min = 0;
    options.max = 40;
    return options;
}

static void startNumeric(const Value &value, NumericKeypadOptions options, bool withCancel = false) {
    g_numericKeypad.init(app(), nullptr, value, options, okFloat, okUint32, withCancel ? cancelCallback : nullptr);
    app()->pushPage(PAGE_ID_GUI_KEYPAD_NUM, &g_numericKeypad);
    harness::frame(2);
    harness::advance(400);
    REQUIRE(getActiveNumericKeypad() == &g_numericKeypad);
}

static std::string numericText() {
    char text[64];
    g_numericKeypad.getText(text, sizeof(text));
    return text;
}

static bool isErrorToastShown(const char *message) {
    if (activePageId() != INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        return false;
    }
    auto toast = (ToastMessagePage *)app()->getActivePage();
    return strcmp(toast->messageValue.getString(), message) == 0;
}

TEST("gui/keypad: the numeric keypad shows the start value until a key is typed") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    CHECK_EQ(keypadText(), std::string("5.0 V"));
    CHECK_EQ(g_numericKeypad.getCursorPosition(), -1);
    CHECK(!g_numericKeypad.isEditing());
}

TEST("gui/keypad: typed digits replace the start value") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("12");
    CHECK_EQ(keypadText(), std::string("12 V"));
    CHECK(g_numericKeypad.isEditing());
}

TEST("gui/keypad: OK of the numeric keypad returns the typed value") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("12");
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okValue, 12.0f);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/keypad: OK before typing returns the start value") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okValue, 5.0f);
}

TEST("gui/keypad: the dot is ignored when it is not enabled") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("1.5");
    CHECK_EQ(keypadText(), std::string("15 V"));
}

TEST("gui/keypad: a decimal value is typed with the dot") {
    auto options = voltOptions();
    options.flags.dotButtonEnabled = true;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    typeNumeric("1.5");
    CHECK_EQ(keypadText(), std::string("1.5 V"));
    tapNumericKey("ok");
    CHECK_EQ(g_okValue, 1.5f);
}

TEST("gui/keypad: a second dot is refused") {
    auto options = voltOptions();
    options.flags.dotButtonEnabled = true;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    typeNumeric("1.5.2");
    CHECK_EQ(keypadText(), std::string("1.52 V"));
}

TEST("gui/keypad: the dot as the first key gives 0.") {
    auto options = voltOptions();
    options.flags.dotButtonEnabled = true;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    typeNumeric(".");
    CHECK_EQ(keypadText(), std::string("0. V"));
    typeNumeric("25");
    tapNumericKey("ok");
    CHECK_EQ(g_okValue, 0.25f);
}

static NumericKeypadOptions signedOptions() {
    auto options = voltOptions();
    options.min = -40;
    options.flags.signButtonEnabled = true;
    return options;
}

TEST("gui/keypad: the sign key adds the minus sign") {
    startNumeric(Value(5.0f, UNIT_VOLT), signedOptions());
    typeNumeric("3");
    tapNumericKey("sign");
    CHECK_EQ(keypadText(), std::string("-3 V"));
    tapNumericKey("ok");
    CHECK_EQ(g_okValue, -3.0f);
}

TEST("gui/keypad: the sign key before the digits starts a negative value") {
    startNumeric(Value(5.0f, UNIT_VOLT), signedOptions());
    tapNumericKey("sign");
    typeNumeric("7");
    CHECK_EQ(keypadText(), std::string("-7 V"));
}

TEST_BUG("gui/keypad: the sign key removes the minus sign", "NEW-gui-3@san") {
    // the minus is removed with stringCopy (strncpy) from the buffer into itself
    startNumeric(Value(5.0f, UNIT_VOLT), signedOptions());
    typeNumeric("3");
    tapNumericKey("sign");
    tapNumericKey("sign");
    CHECK_EQ(keypadText(), std::string("3 V"));
}

TEST("gui/keypad: the sign key does nothing when it is not enabled") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("3");
    tapNumericKey("sign");
    CHECK_EQ(keypadText(), std::string("3 V"));
}

TEST("gui/keypad: back deletes the last typed character") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("123");
    tapNumericKey("back");
    CHECK_EQ(keypadText(), std::string("12 V"));
}

TEST("gui/keypad: OK with an empty text does nothing") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("1");
    tapNumericKey("back");
    CHECK_EQ(keypadText(), std::string(" V"));
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 0);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_KEYPAD_NUM);
}

TEST("gui/keypad: back before typing empties the text") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    tapNumericKey("back");
    CHECK_EQ(keypadText(), std::string(" V"));
}

TEST("gui/keypad: clear returns to the start value") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("12");
    tapNumericKey("clear");
    CHECK_EQ(keypadText(), std::string("5.0 V"));
    tapNumericKey("ok");
    CHECK_EQ(g_okValue, 5.0f);
}

TEST("gui/keypad: the unit key switches the edit unit and the value is converted") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    CHECK_STR(data(data_keypad_edit_unit).getString(), "mV");
    tapNumericKey("unit");
    CHECK_EQ(g_numericKeypad.getEditUnit(), UNIT_MILLI_VOLT);
    CHECK_STR(data(data_keypad_edit_unit).getString(), "V");
    typeNumeric("500");
    CHECK_EQ(keypadText(), std::string("500 mV"));
    tapNumericKey("ok");
    CHECK_NEAR(g_okValue, 0.5, 1e-6);
}

TEST("gui/keypad: the edit unit of a small start value is the derived unit") {
    startNumeric(Value(0.05f, UNIT_VOLT), voltOptions());
    CHECK_EQ(g_numericKeypad.getEditUnit(), UNIT_MILLI_VOLT);
    typeNumeric("20");
    tapNumericKey("ok");
    CHECK_NEAR(g_okValue, 0.02, 1e-6);
}

TEST("gui/keypad: a value above max shows an error and is not returned") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("50");
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 0);
    CHECK(isErrorToastShown("Greater than max. allowed value"));
}

TEST("gui/keypad: a value below min shows an error and is not returned") {
    auto options = voltOptions();
    options.min = 1;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    typeNumeric("0");
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 0);
    CHECK(isErrorToastShown("Less than min. allowed value"));
}

TEST("gui/keypad: zero below min is returned when zero is allowed") {
    auto options = voltOptions();
    options.min = 1;
    options.allowZero = true;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    typeNumeric("0");
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okValue, 0.0f);
}

TEST("gui/keypad: min and max themselves are returned") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    typeNumeric("40");
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okValue, 40.0f);
}

TEST("gui/keypad: at most 16 characters are typed in the numeric keypad") {
    NumericKeypadOptions options;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    for (int i = 0; i < 17; i++) {
        g_numericKeypad.key('1');
    }
    CHECK_EQ(numericText(), std::string("1111111111111111 V"));
}

TEST_BUG("gui/keypad: the keypad text of 15 typed digits keeps its unit", "G17") {
    NumericKeypadOptions options;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    for (int i = 0; i < 15; i++) {
        g_numericKeypad.key('1');
    }
    CHECK_EQ(keypadText(), std::string("111111111111111 V"));
}

TEST("gui/keypad: a value with ten integer digits is greater than max") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    // 4294967300 wraps to 4 in the int accumulator
    typeNumeric("4294967300");
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 0);
    CHECK(isErrorToastShown("Greater than max. allowed value"));
}

TEST("gui/keypad: cancel of the numeric keypad without a callback closes it") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions());
    tapNumericKey("cancel");
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
    CHECK_EQ(g_okCount, 0);
}

TEST("gui/keypad: cancel of the numeric keypad calls the cancel callback") {
    startNumeric(Value(5.0f, UNIT_VOLT), voltOptions(), true);
    tapNumericKey("cancel");
    CHECK_EQ(g_cancelCount, 1);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/keypad: the max, min and def options") {
    auto options = voltOptions();
    options.def = 12;
    options.enableMaxButton();
    options.enableMinButton();
    options.enableDefButton();
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    CHECK_STR(data(data_keypad_option1_text).getString(), "max");
    CHECK_EQ(data(data_keypad_option1_enabled).getInt(), 1);
    CHECK_STR(data(data_keypad_option2_text).getString(), "min");
    CHECK_STR(data(data_keypad_option3_text).getString(), "def");
    g_numericKeypad.setMaxValue();
    CHECK_EQ(g_okValue, 40.0f);
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    g_numericKeypad.setMinValue();
    CHECK_EQ(g_okValue, 0.0f);
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    g_numericKeypad.setDefValue();
    CHECK_EQ(g_okValue, 12.0f);
}

TEST("gui/keypad: the data items of the numeric keypad") {
    auto options = voltOptions();
    options.flags.signButtonEnabled = true;
    startNumeric(Value(5.0f, UNIT_VOLT), options);
    CHECK_EQ(data(data_keypad_sign_enabled).getInt(), 1);
    CHECK_EQ(data(data_keypad_dot_enabled).getInt(), 0);
    CHECK_EQ(data(data_keypad_option1_enabled).getInt(), 0);
    CHECK_STR(data(data_keypad_option1_text).getString(), "");
    CHECK_EQ(data(data_keypad_ok_enabled).getInt(), 1);
    Value text = data(data_keypad_text);
    CHECK_STR(text.getString(), "5.0 V");
    // no keypad: nothing
    app()->popPage();
    CHECK_EQ(data(data_keypad_text).getType(), VALUE_TYPE_UNDEFINED);
}

TEST("gui/keypad: an IP address is edited from its text") {
    uint32_t ipAddress = getIpAddress(192, 168, 1, 10);
    startNumeric(Value(ipAddress, VALUE_TYPE_IP_ADDRESS), NumericKeypadOptions());
    CHECK_EQ(data(data_keypad_dot_enabled).getInt(), 1);
    CHECK(g_numericKeypad.isEditing());
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okUint32Value, ipAddress);
}

TEST("gui/keypad: a typed IP address is returned") {
    startNumeric(Value(getIpAddress(192, 168, 1, 10), VALUE_TYPE_IP_ADDRESS), NumericKeypadOptions());
    for (int i = 0; i < 12; i++) {
        tapNumericKey("back");
    }
    typeNumeric("10.0.0.1");
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okUint32Value, getIpAddress(10, 0, 0, 1));
}

TEST("gui/keypad: an invalid IP address shows an error") {
    startNumeric(Value(getIpAddress(192, 168, 1, 10), VALUE_TYPE_IP_ADDRESS), NumericKeypadOptions());
    for (int i = 0; i < 6; i++) {
        tapNumericKey("back");
    }
    tapNumericKey("ok");
    CHECK_EQ(g_okCount, 0);
    CHECK(isErrorToastShown("Invalid IP address format!"));
}

////////////////////////////////////////////////////////////////////////////////
// text keypad

static Keypad g_textKeypad;
static std::string g_okText;

static void okText(char *text) {
    g_okCount++;
    g_okText = text;
    app()->popPage();
}

static void startText(const char *label, const char *text, int minChars, int maxChars, bool password = false, bool withCancel = false) {
    g_textKeypad.start(app(), label, text, minChars, maxChars, password, okText, withCancel ? cancelCallback : nullptr, nullptr);
    app()->pushPage(PAGE_ID_GUI_KEYPAD_TEXT, &g_textKeypad);
    harness::frame(2);
    harness::advance(400);
    REQUIRE(getActiveKeypad() == &g_textKeypad);
}

TEST("gui/keypad: letters are typed in the text keypad") {
    startText(nullptr, "", 0, 20);
    tapTextKey("a");
    tapTextKey("b");
    tapTextKey("c");
    CHECK_EQ(keypadText(), std::string("abc"));
    CHECK_EQ(g_textKeypad.getCursorPosition(), 3);
}

TEST("gui/keypad: space, back and clear of the text keypad") {
    startText(nullptr, "", 0, 20);
    tapTextKey("a");
    tapTextKey("space");
    tapTextKey("b");
    CHECK_EQ(keypadText(), std::string("a b"));
    tapTextKey("back");
    CHECK_EQ(keypadText(), std::string("a "));
    tapTextKey("clear");
    CHECK_EQ(keypadText(), std::string(""));
}

TEST("gui/keypad: OK of the text keypad passes the text") {
    startText(nullptr, "", 0, 20);
    tapTextKey("b");
    tapTextKey("a");
    tapTextKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okText, std::string("ba"));
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/keypad: cancel of the text keypad calls the cancel callback") {
    startText(nullptr, "", 0, 20, false, true);
    tapTextKey("cancel");
    CHECK_EQ(g_cancelCount, 1);
    CHECK_EQ(g_okCount, 0);
}

TEST("gui/keypad: cancel of the text keypad without a callback closes it") {
    startText(nullptr, "", 0, 20);
    tapTextKey("cancel");
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/keypad: the initial text is edited at its end") {
    startText(nullptr, "xy", 0, 20);
    CHECK_EQ(g_textKeypad.getCursorPosition(), 2);
    tapTextKey("a");
    CHECK_EQ(keypadText(), std::string("xya"));
}

TEST("gui/keypad: at most maxChars characters are typed") {
    startText(nullptr, "", 0, 3);
    tapTextKey("a");
    tapTextKey("b");
    tapTextKey("c");
    tapTextKey("a");
    CHECK_EQ(keypadText(), std::string("abc"));
}

TEST("gui/keypad: OK is disabled with less than minChars characters") {
    startText(nullptr, "", 2, 20);
    tapTextKey("a");
    CHECK_EQ(data(data_keypad_ok_enabled).getInt(), 0);
    tapTextKey("ok");
    CHECK_EQ(g_okCount, 0);
    tapTextKey("b");
    CHECK_EQ(data(data_keypad_ok_enabled).getInt(), 1);
    tapTextKey("ok");
    CHECK_EQ(g_okCount, 1);
    CHECK_EQ(g_okText, std::string("ab"));
}

TEST("gui/keypad: the label is shown before the text") {
    startText("Name: ", "ab", 0, 20);
    CHECK_EQ(keypadText(), std::string("Name: ab"));
    CHECK_STR(data(data_keypad_label).getString(), "Name: ");
    // the cursor position includes the label
    CHECK_EQ(g_textKeypad.getCursorPosition(), 8);
    tapTextKey("ok");
    CHECK_EQ(g_okText, std::string("ab"));
}

TEST("gui/keypad: a password shows its last character for 500 ms") {
    startText(nullptr, "", 0, 20, true);
    g_textKeypad.key('a');
    g_textKeypad.key('b');
    CHECK_EQ(keypadText(), std::string("*b"));
    harness::advance(501);
    CHECK_EQ(keypadText(), std::string("**"));
    g_textKeypad.ok();
    CHECK_EQ(g_okText, std::string("ab"));
}

TEST("gui/keypad: the mode key cycles lowercase, uppercase and symbols") {
    startText(nullptr, "", 0, 20);
    CHECK_EQ(data(data_keypad_mode).getInt(), (int)KEYPAD_MODE_LOWERCASE);
    tapTextKey("mode");
    CHECK_EQ((int)g_textKeypad.m_keypadMode, (int)KEYPAD_MODE_UPPERCASE);
    tapTextKey("mode");
    CHECK_EQ((int)g_textKeypad.m_keypadMode, (int)KEYPAD_MODE_SYMBOL);
    tapTextKey("mode");
    CHECK_EQ((int)g_textKeypad.m_keypadMode, (int)KEYPAD_MODE_LOWERCASE);
}

TEST("gui/keypad: a touch on the keypad text moves the cursor") {
    // keypad text: left aligned, border 1, padding 4: the text starts at x = 5
    startText(nullptr, "abc", 0, 20);
    tap(6, 30);
    CHECK_EQ(g_textKeypad.getCursorPosition(), 0);
    tapTextKey("b");
    CHECK_EQ(keypadText(), std::string("babc"));
    tap(700, 30);
    CHECK_EQ(g_textKeypad.getCursorPosition(), 4);
}

TEST("gui/keypad: the keypad text cursor position data") {
    startText("L:", "abc", 0, 20);
    CHECK_EQ(data(data_keypad_text, DATA_OPERATION_GET_TEXT_CURSOR_POSITION).getInt(), 5);
}

TEST("gui/keypad: a password with a long label and initial text fits the text buffer") {
    // flow ShowKeyboard with password, a label and a long initial text
    std::string label(60, 'L');
    std::string text(100, 'x');
    startText(label.c_str(), text.c_str(), 0, 128, true);
    Value value = g_textKeypad.getKeypadTextValue();
    CHECK(strlen(value.getString()) <= MAX_KEYPAD_TEXT_LENGTH);
}

TEST_BUG("gui/keypad: back deletes a whole UTF-8 character", "G13") {
    startText(nullptr, "a\xc3\xb1", 0, 20);
    tapTextKey("back");
    CHECK_EQ(keypadText(), std::string("a"));
}

TEST_BUG("gui/keypad: a touch at the end of a UTF-8 text puts the cursor at the end", "G13") {
    startText(nullptr, "\xc3\xb1\xc3\xb1", 0, 20);
    tap(700, 30);
    tapTextKey("a");
    CHECK_EQ(keypadText(), std::string("\xc3\xb1\xc3\xb1" "a"));
}

TEST_BUG("gui/keypad: a key with a non-ASCII label inserts the whole character", "G13") {
    startText(nullptr, "", 0, 20);
    tapTextKey("n~");
    CHECK_EQ(keypadText(), std::string("\xc3\xb1"));
}
