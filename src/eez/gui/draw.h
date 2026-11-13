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

namespace eez {
namespace gui {

inline font::Font styleGetFont(const Style *style) {
    font::Font font(getFontData(style->font));
    font.letterSpacing = style->letterSpacing;
    return font;
}

inline bool styleIsBlink(const Style *style) {
    return style->flags & STYLE_FLAGS_BLINK;
}

// The style of the widget in its states (STYLE_STATE_FLAG bits): the state
// style of the first active state in the order disabled, pressed, checked,
// selected, focused that the style has, or the style itself. appliedState gets
// the STYLE_STATE_FLAG of the used state style, 0 if none.
const Style *getStateStyle(const Style *style, unsigned states, unsigned *appliedState = nullptr);

// true if the style has a style for the state
inline bool styleHasStateStyle(const Style *style, StyleState state) {
    return style->stateStyles[state] != 0;
}

// How a widget is drawn in its states (see getStateStyle).
struct WidgetStateStyle {
    // the style of the widget in its states
    const Style *style;
    // pressed and the style has no style for the pressed state: drawn active,
    // with the color and the background swapped (see swapColorsIfActive)
    bool active;
};

// the style of a widget pressed and/or focused, and in the other states
// (STYLE_STATE_FLAG bits). Focused without a style for the focused state is
// drawn as not focused.
WidgetStateStyle getWidgetStateStyle(const Style *style, bool pressed, bool focused = false, unsigned otherStates = 0);

// the STYLE_STATE_FLAG of the disabled state, if the widget is disabled (see
// isWidgetActionDisabled)
inline unsigned getDisabledStateFlag(bool disabled) {
    return disabled ? STYLE_STATE_FLAG(STYLE_STATE_DISABLED) : 0;
}

// A widget drawn active (pressed without a style for the pressed state, see
// WidgetStateStyle::active) has its color and background color swapped. Over a
// transparent background the colors stay: such a style needs a style for the
// pressed state for a pressed look.
void swapColorsIfActive(bool active, uint16_t &color, uint16_t &backgroundColor);
// the color and the background color of the style, swapped if active
uint16_t getStyleColor(const Style *style, bool active);
uint16_t getStyleBackgroundColor(const Style *style, bool active);

// A blinking widget (the blink property of its style or blinking data) fades
// out and in, its opacity is g_blinkOpacity. Multiplies the display opacity by
// blinkOpacity while the widget is drawn (from the constructor to the
// destructor).
struct BlinkOpacity {
    uint8_t savedOpacity;
    explicit BlinkOpacity(uint8_t blinkOpacity);
    ~BlinkOpacity();
};

// The focus ring of the style (focusRingWidth, focusRingColor) inside the box
// x, y, w, h, focusRingOffset px from its edge; its rounded corners follow the
// outer edge of the box (the border radius and width of the style).
void drawFocusRing(int x, int y, int w, int h, const Style *style);

// Keeps what the children of a widget with rounded corners draw inside of its
// border and its rounded corners (see display::RoundedClip): begin before the
// children are enumerated, end after them.
struct ChildrenRoundedClip {
    display::RoundedClip clip;
    display::ClipRect savedClipRect;

    void begin(const WidgetCursor &widgetCursor, const Style *style);
    void end();
};

// true if the style has a visible shadow
bool styleHasShadow(const Style *style);
// the box x1, y1, x2, y2 grown by the shadow of the style
void expandRectWithStyleShadow(const Style *style, int &x1, int &y1, int &x2, int &y2);
// The shadow of the style around the box x1, y1, x2, y2 (only outside of the
// box), with the radius of the top left corner of the style.
void drawStyleShadow(const Style *style, int x1, int y1, int x2, int y2);

// The border and the background of the style in the box, which is then
// shrunk to the inside of the border. insideWidget: a box drawn over the
// background of the current widget (e.g. a part of it), the backgrounds below
// the widget are not drawn first.
void drawBorderAndBackground(int &x1, int &y1, int &x2, int &y2, const Style *style, uint16_t color, bool ignoreLuminocity = false, bool insideWidget = false);

// active: the color and the background swapped (see swapColorsIfActive), also
// overrideColor and overrideBackgroundColor
void drawText(
    const char *text, int textLength,
    int x, int y, int w, int h,
    const Style *style,
    bool active = false, bool ignoreLuminocity = false,
    uint16_t *overrideColor = nullptr, uint16_t *overrideBackgroundColor = nullptr,
    bool useSmallerFontIfDoesNotFit = false, int cursorPosition = -1, int xScroll = 0,
	bool boolSkipBackground = false
);
// The y of the baseline of the text drawText draws in a box of the height h
// (also of the integer part of drawIntegerUnitAndFraction), from the top of
// the box; -1 if the style has no font.
int getTextBaseline(const Style *style, int h);
// Integer part with the style font, unit (top, above the first fraction digit) and fraction
// (integer baseline) with unitAndFractionFont (style font if not set) in a column right of the
// integer part.
void drawIntegerUnitAndFraction(
    const char *integer, int integerLength,
    const char *unit, int unitLength,
    const char *fraction, int fractionLength,
    int x, int y, int w, int h,
    const Style *style, font::Font unitAndFractionFont,
    bool active,
    uint16_t *overrideColor, uint16_t *overrideBackgroundColor
);
// A value with a label above it and a unit after it (a readout or a set field,
// see the DisplayData and Input widgets). A part that is not shown is null, a
// style that is not set is null.
struct ValueParts {
    // a line above the value line in labelStyle: its font and color, its
    // padding around the label and its horizontal alignment in the content box
    // of the widget; the font, color and alignment of the widget style if
    // labelStyle is not set
    const char *label = nullptr;
    const Style *labelStyle = nullptr;
    const char *value = nullptr;
    int valueLength = -1;
    // after the value, on its baseline, in unitStyle: its font and color, its
    // left padding is the gap after the value; the widget style and the width
    // of a space if unitStyle is not set
    const char *unit = nullptr;
    int unitLength = 0;
    const Style *unitStyle = nullptr;
};
// The parts in the box x, y, w, h with the border, background and padding of
// the style. The label and the value line are one block placed by the vertical
// alignment of the style, the value and the unit are placed by its horizontal
// alignment. Right aligned, the value ends at the right of the content box and
// the unit is in the right padding, so the values of a
// column of widgets end at one x whatever their units (the unit and the value
// move left if the unit doesn't fit). active: as in drawText.
// useSmallerFontIfDoesNotFit: a smaller font of the value (on the same
// baseline) if the value and the unit don't fit.
void drawValueParts(
    const ValueParts &parts,
    int x, int y, int w, int h,
    const Style *style,
    bool active,
    uint16_t *overrideColor, uint16_t *overrideBackgroundColor,
    bool useSmallerFontIfDoesNotFit = false
);
// The size of the parts as drawValueParts draws them, with the padding and the
// border of the style; false if the style has no font.
bool measureValueParts(const ValueParts &parts, const Style *style, int &w, int &h);
// The y of the baseline of the value as drawValueParts draws it in a box of
// the height h, from the top of the box; -1 if the style has no font.
int getValuePartsBaseline(const ValueParts &parts, const Style *style, int h);

int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int w, int h, const Style *style);
int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int w, int h, const Style *style);

void drawMultilineText(const char *text, int x, int y, int w, int h, const Style *style, bool active, int firstLineIndent, int hangingIndent);
int measureMultilineText(const char *text, int x, int y, int w, int h, const Style *style, int firstLineIndent, int hangingIndent);

void drawBitmap(Image *image, int x, int y, int w, int h, const Style *style, bool active);
void drawRectangle(int x, int y, int w, int h, const Style *style, bool active = false, bool ignoreLuminocity = false, bool invertColors = true);

// CSS box-shadow like parameters, used only by the software shadow renderer (SDL: simulator and CM5)
struct ShadowParams {
    int offsetX;
    int offsetY;
    int blur;         // blur radius in px (gaussian sigma = blur / 2)
    int spread;       // grow (positive) or shrink (negative) shadow box in px
    int cornerRadius; // shadow box corner radius in px
    uint8_t r, g, b;
    uint8_t opacity;
};

void setShadowParams(const ShadowParams &params);
const ShadowParams &getShadowParams();

// The shadow around the box x1, y1, x2, y2: outside of the box and in the
// squares of the shadow corner radius inside its corners. underBox: also below
// the whole box (the background of a page with rounded corners: the shadow
// shows outside of its corners).
void drawShadow(int x1, int y1, int x2, int y2, bool underBox = false);
void expandRectWithShadow(int &x1, int &y1, int &x2, int &y2);

void drawLine(int x1, int y1, int x2, int y2);
void drawAntialiasedLine(int x1, int y1, int x2, int y2);

// Checkbox (rounded square, check mark if checked) or radio button (circle,
// filled circle if checked) with the label right of it after a space. The box
// is 3/4 of the style font height, as big as an icon glyph of the font. The box
// and the label are aligned in the rectangle as a text in drawText and drawn
// with the style color on the style background (swapped if active).
void drawCheckboxOrRadio(int x, int y, int w, int h, const Style *style, bool radio, bool checked, const char *label, bool active = false);
// width of the box and the label, without the padding
int measureCheckboxOrRadio(const Style *style, const char *label);

} // namespace gui
} // namespace eez
