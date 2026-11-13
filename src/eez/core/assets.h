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

#include <stdint.h>

#include <eez/core/memory.h>
#include <eez/core/value.h>

namespace eez {

static const uint32_t HEADER_TAG = 0x5A45457E; // "~EEZ"
static const uint32_t HEADER_TAG_COMPRESSED = 0x7A65657E; // "~eez"

static const uint8_t PROJECT_VERSION_V2 = 2;
static const uint8_t PROJECT_VERSION_V3 = 3;

static const uint8_t ASSETS_TYPE_FIRMWARE = 1;
static const uint8_t ASSETS_TYPE_FIRMWARE_MODULE = 2;
static const uint8_t ASSETS_TYPE_RESOURCE = 3;
static const uint8_t ASSETS_TYPE_APPLET = 4;
static const uint8_t ASSETS_TYPE_DASHBOARD = 5;

struct Header {
	uint32_t tag; // HEADER_TAG or HEADER_TAG_COMPRESSED
	uint8_t projectMajorVersion;
	uint8_t projectMinorVersion;
	uint8_t assetsType;
    uint8_t reserved;
	uint32_t decompressedSize;
};

struct Assets;
extern Assets *g_mainAssets;
extern bool g_mainAssetsAreMutable;

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct AssetsPtr {
    AssetsPtr() : offset(0) {}
    AssetsPtr(const AssetsPtr<T> &rhs) = delete;

	void operator=(T* ptr) {
		if (ptr != nullptr) {
            offset = (uint8_t *)ptr - (uint8_t *)&offset;
		} else {
			offset = 0;
		}
    }

    /* Conversion to a T pointer */
    operator T*() { return ptr(); }
    operator const T*() const { return ptr(); }
    /* Dereferencing operators */
          T* operator->()       { return ptr(); }
    const T* operator->() const { return ptr(); }

private:
    int32_t offset;

    T* ptr() {
        return offset ? (T *)((uintptr_t)&offset + offset) : nullptr;
    }

	const T* ptr() const {
		return offset ? (const T *)((uintptr_t)&offset + offset) : nullptr;
	}
};

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct ListOfAssetsPtr {
	uint32_t count = 0;

    /* Array access */
    T*       operator[](uint32_t i)       { return item(i); }
    const T* operator[](uint32_t i) const { return item(i); }

private:
    AssetsPtr<AssetsPtr<T>> items;

    T* item(int i) {
        return static_cast<T *>(static_cast<AssetsPtr<T> *>(items)[i]);
    }

    const T* item(int i) const {
        return static_cast<const T *>(static_cast<const AssetsPtr<T> *>(items)[i]);
    }
};

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct ListOfFundamentalType {
	uint32_t count;
    AssetsPtr<T> items;

    /* Array access */
    T&       operator[](uint32_t i)       { return ptr()[i]; }
    const T& operator[](uint32_t i) const { return ptr()[i]; }

private:
    T *ptr() {
        return static_cast<T *>(items);
    }
    const T *ptr() const {
        return static_cast<const T *>(items);
    }
};

////////////////////////////////////////////////////////////////////////////////

struct Settings {
    uint16_t displayWidth;
    uint16_t displayHeight;
};

////////////////////////////////////////////////////////////////////////////////

#if EEZ_OPTION_GUI

#define WIDGET_FLAG_PIN_TO_LEFT (1 << 0)
#define WIDGET_FLAG_PIN_TO_RIGHT (1 << 1)
#define WIDGET_FLAG_PIN_TO_TOP (1 << 2)
#define WIDGET_FLAG_PIN_TO_BOTTOM (1 << 3)

#define WIDGET_FLAG_FIX_WIDTH (1 << 4)
#define WIDGET_FLAG_FIX_HEIGHT (1 << 5)

#define WIDGET_TIMELINE_PROPERTY_X (1 << 0)
#define WIDGET_TIMELINE_PROPERTY_Y (1 << 1)
#define WIDGET_TIMELINE_PROPERTY_WIDTH (1 << 2)
#define WIDGET_TIMELINE_PROPERTY_HEIGHT (1 << 3)
#define WIDGET_TIMELINE_PROPERTY_OPACITY (1 << 4)
#define WIDGET_TIMELINE_PROPERTY_CP1 (1 << 5)
#define WIDGET_TIMELINE_PROPERTY_CP2 (1 << 6)

#define EASING_FUNC_LINEAR 0
#define EASING_FUNC_IN_QUAD 1
#define EASING_FUNC_OUT_QUAD 2
#define EASING_FUNC_IN_OUT_QUAD 3
#define EASING_FUNC_IN_CUBIC 4
#define EASING_FUNC_OUT_CUBIC 5
#define EASING_FUNC_IN_OUT_CUBIC 6
#define EASING_FUNC_IN__QUART 7
#define EASING_FUNC_OUT_QUART 8
#define EASING_FUNC_IN_OUT_QUART 9
#define EASING_FUNC_IN_QUINT 10
#define EASING_FUNC_OUT_QUINT 11
#define EASING_FUNC_IN_OUT_QUINT 12
#define EASING_FUNC_IN_SINE 13
#define EASING_FUNC_OUT_SINE 14
#define EASING_FUNC_IN_OUT_SINE 15
#define EASING_FUNC_IN_EXPO 16
#define EASING_FUNC_OUT_EXPO 17
#define EASING_FUNC_IN_OUT_EXPO 18
#define EASING_FUNC_IN_CIRC 19
#define EASING_FUNC_OUT_CIRC 20
#define EASING_FUNC_IN_OUT_CIRC 21
#define EASING_FUNC_IN_BACK 22
#define EASING_FUNC_OUT_BACK 23
#define EASING_FUNC_IN_OUT_BACK 24
#define EASING_FUNC_IN_ELASTIC 25
#define EASING_FUNC_OUT_ELASTIC 26
#define EASING_FUNC_IN_OUT_ELASTIC 27
#define EASING_FUNC_IN_BOUNCE 28
#define EASING_FUNC_OUT_BOUNCE 29
#define EASING_FUNC_IN_OUT_BOUNCE 30

namespace gui {

struct TimelineKeyframe {
    float start;
    float end;

    uint32_t enabledProperties;

	int16_t x;
	int16_t y;

	int16_t width;
	int16_t height;

    float opacity;

    uint8_t xEasingFunc;
    uint8_t yEasingFunc;
    uint8_t widthEasingFunc;
    uint8_t heightEasingFunc;

    uint8_t opacityEasingFunc;
    uint8_t reserved1;
    uint16_t reserved2;

	int16_t cp1x;
	int16_t cp1y;
    int16_t cp2x;
    int16_t cp2y;
};

struct Widget {
	uint16_t type;
	int16_t data;
    int16_t visible;
	int16_t action;
	int16_t x;
	int16_t y;
	int16_t width;
	int16_t height;
	int16_t style;
    uint16_t flags;
    ListOfAssetsPtr<TimelineKeyframe> timeline;
};

#define SHADOW_FLAG (1 << 0)
#define CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG (1 << 1)
#define PAGE_IS_USED_AS_USER_WIDGET (1 << 2)
#define PAGE_CONTAINER (1 << 3)
#define PAGE_SCALE_TO_FIT (1 << 4)
// children of the container (or page) with static layout are allowed to overlap
#define OVERLAPPING_CHILDREN_FLAG (1 << 5)

struct PageAsset : public Widget {
	ListOfAssetsPtr<Widget> widgets;
	uint16_t flags;
	int16_t overlay;
};

////////////////////////////////////////////////////////////////////////////////

#define STYLE_FLAGS_HORZ_ALIGN_MASK 0x7
#define STYLE_FLAGS_HORZ_ALIGN_LEFT 0
#define STYLE_FLAGS_HORZ_ALIGN_RIGHT 1
#define STYLE_FLAGS_HORZ_ALIGN_CENTER 2

#define STYLE_FLAGS_VERT_ALIGN_MASK (0x7 << 3)
#define STYLE_FLAGS_VERT_ALIGN_TOP (0 << 3)
#define STYLE_FLAGS_VERT_ALIGN_BOTTOM (1 << 3)
#define STYLE_FLAGS_VERT_ALIGN_CENTER (2 << 3)

#define STYLE_FLAGS_BLINK (1 << 6)
// single-line text that doesn't fit is cut and ends with an ellipsis
#define STYLE_FLAGS_ELLIPSIS (1 << 7)

// The states of a widget that can have their own style (Style::stateStyles).
// When more of them are active, the style of the first one in the order
// disabled, pressed, checked, selected, focused that has a style is used (see
// getStateStyle).
enum StyleState {
    STYLE_STATE_PRESSED,
    STYLE_STATE_FOCUSED,
    STYLE_STATE_DISABLED,
    STYLE_STATE_SELECTED,
    STYLE_STATE_CHECKED,
    STYLE_STATE_COUNT
};

#define STYLE_STATE_FLAG(state) (1 << (state))

struct Style {
    uint16_t flags; // STYLE_FLAGS_...

    uint16_t backgroundColor;
    uint16_t color;

    uint8_t borderSizeTop;
    uint8_t borderSizeRight;
    uint8_t borderSizeBottom;
    uint8_t borderSizeLeft;

    uint16_t borderColor;

    // in the order Studio writes them: top left, top right, bottom right, bottom left
    uint8_t borderRadiusTLX;
	uint8_t borderRadiusTLY;
    uint8_t borderRadiusTRX;
	uint8_t borderRadiusTRY;
    uint8_t borderRadiusBRX;
	uint8_t borderRadiusBRY;
    uint8_t borderRadiusBLX;
	uint8_t borderRadiusBLY;

    int8_t font; // > 0 main assets font, < 0 external assets font (see getFontData)
    uint8_t opacity; // 0 - 255

    uint8_t paddingTop;
    uint8_t paddingRight;
    uint8_t paddingBottom;
    uint8_t paddingLeft;

	int16_t backgroundImage;

    // Styles of the widget states (StyleState), 0 if the state has no style:
    // style ids as Widget::style (positive in the main assets, negative in the
    // external assets). Studio writes the style with the state properties over
    // this style (and these are 0 in a state style).
    int16_t stateStyles[STYLE_STATE_COUNT];

    // extra space between the characters of the text, in 1/64 px
    int16_t letterSpacing;
    // distance between the lines of multiline text in px, 0: 90 % of the
    // font height
    uint16_t lineHeight;

    // the ring drawn when the widget has the focus, inside the widget box at
    // focusRingOffset px from its edge (following the border radius)
    uint16_t focusRingColor;
    uint8_t focusRingWidth; // 0: no focus ring
    uint8_t focusRingOffset;

    // shadow of the widget (CSS box-shadow), drawn outside of the widget box,
    // inside of its parent; only on displays with a 32-bit buffer
    uint16_t shadowColor;
    int8_t shadowOffsetX;
    int8_t shadowOffsetY;
    uint8_t shadowBlur;
    int8_t shadowSpread;
};

////////////////////////////////////////////////////////////////////////////////

struct GlyphData {
	int8_t dx;         // DWIDTH (-128 indicated empty glyph)
	uint8_t width;     // BBX width
	uint8_t height;    // BBX height
	int8_t x;          // BBX xoffset
	int8_t y;          // BBX yoffset
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
	uint8_t pixels[1];
};

struct GlyphsGroup {
    uint32_t encoding;
    uint32_t glyphIndex;
    uint32_t length;
};

// FontData::kind
#define FONT_KIND_BITMAP 0 // the glyphs are in the assets (FontData)
#define FONT_KIND_VECTOR 1 // drawn from a font file at runtime (VectorFontData)

struct FontData {
	uint8_t ascent;
	uint8_t descent;
    uint8_t kind;
    uint8_t reserved2;
	uint32_t encodingStart;
	uint32_t encodingEnd;
    ListOfAssetsPtr<GlyphsGroup> groups;
    ListOfAssetsPtr<GlyphData> glyphs;
};

// A TrueType or OpenType font file, stored once for all the vector fonts made
// from it.
struct FontFile {
    uint32_t size;
    uint8_t data[1];
};

// A font of kind FONT_KIND_VECTOR (see EEZ_OPTION_GUI_VECTOR_FONTS): the font
// file at a size, with OpenType features. The ascent and descent are the
// pixels above and below the baseline, as of a bitmap font.
struct VectorFontData {
	uint8_t ascent;
	uint8_t descent;
    uint8_t kind;
    uint8_t reserved;
    AssetsPtr<FontFile> file;
    // pixels per em, in 1/64 px
    uint32_t size;
    // added after every glyph, in 1/64 px
    int32_t letterSpacing;
    // comma separated OpenType features as HarfBuzz parses them (e.g.
    // "tnum,-liga"), null for the default features
    AssetsPtr<const char> features;
};

////////////////////////////////////////////////////////////////////////////////

struct Bitmap {
    int16_t w;
    int16_t h;
    int16_t bpp;
    int16_t reserved;
    AssetsPtr<const char> name;
    const uint8_t pixels[1];
};

} // namespace gui

#endif // EEZ_OPTION_GUI

////////////////////////////////////////////////////////////////////////////////

// The theme colors and the custom colors are 32-bit colors (R in the low
// byte, alpha in the high byte) in assets with 32-bit colors (Studio's "Color
// BPP" 32, projectMinorVersion 1), and RGB565 (uint16_t) values in the other
// assets. Read them with display::getColorFromIndex.
struct Theme {
	AssetsPtr<const char> name;
	ListOfFundamentalType<uint32_t> colors;
};

struct Colors {
	ListOfAssetsPtr<Theme> themes;
	ListOfFundamentalType<uint32_t> colors;
};

////////////////////////////////////////////////////////////////////////////////

static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_MASK  = 0x0007 << 13;
static const uint16_t EXPR_EVAL_INSTRUCTION_PARAM_MASK = 0xFFFF >> 3;

static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT           = (0 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT              = (1 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR          = (2 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR         = (3 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_OUTPUT             = (4 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_ARRAY_ELEMENT                = (5 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_OPERATION               = (6 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_END                     = (7 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_END_WITH_DST_VALUE_TYPE = (7 << 13) | (1 << 12);

// Type 7 with bit 11 set and bit 12 clear: a jump, the kind is in the low bits.
// The instruction is followed by one uint16 operand, a forward offset in 16-bit
// words counted from the word after the operand.
//   t ? c : a  ->  [t] [JUMP_IF_FALSE off1] [c] [JUMP off2] [a]
//                  (off1 points at the JUMP off2 instruction)
//   a && b     ->  [a] [JUMP_AND off] [b] [OPERATION LOGICAL_AND]
//   a || b     ->  [a] [JUMP_OR off] [b] [OPERATION LOGICAL_OR]
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_JUMP_MASK = (7 << 13) | (1 << 12) | (1 << 11);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_JUMP      = (7 << 13) | (1 << 11);

static const uint16_t EXPR_EVAL_INSTRUCTION_JUMP          = EXPR_EVAL_INSTRUCTION_TYPE_JUMP | 0;
static const uint16_t EXPR_EVAL_INSTRUCTION_JUMP_IF_FALSE = EXPR_EVAL_INSTRUCTION_TYPE_JUMP | 1;
static const uint16_t EXPR_EVAL_INSTRUCTION_JUMP_AND      = EXPR_EVAL_INSTRUCTION_TYPE_JUMP | 2;
static const uint16_t EXPR_EVAL_INSTRUCTION_JUMP_OR       = EXPR_EVAL_INSTRUCTION_TYPE_JUMP | 3;

struct Property {
	uint8_t evalInstructions[1];
};

struct Connection {
	uint16_t targetComponentIndex;
	uint16_t targetInputIndex;
};

struct ComponentOutput {
	ListOfAssetsPtr<Connection> connections;
	uint32_t isSeqOut;
};

static const uint16_t BREAKPOINT_ENABLED = 1;
static const uint16_t BREAKPOINT_DISABLED = 2;

struct Component {
    uint16_t type;
    uint16_t breakpoint;

	// These are indexes to Flow::componentInputs.
	// We use this to check if component is ready to run (i.e. all mandatory inputs have a value).
	ListOfFundamentalType<uint16_t> inputs;

	ListOfAssetsPtr<Property> properties;
	ListOfAssetsPtr<ComponentOutput> outputs;
	int16_t errorCatchOutput;
	uint16_t reserved;
};

struct WidgetDataItem {
	int16_t componentIndex;
	int16_t propertyValueIndex;
};

struct WidgetActionItem {
	int16_t componentIndex;
	int16_t componentOutputIndex;
};

#define COMPONENT_INPUT_FLAG_IS_SEQ_INPUT   (1 << 0)
#define COMPONENT_INPUT_FLAG_IS_OPTIONAL (1 << 1)
typedef uint8_t ComponentInput;

struct Flow {
	ListOfAssetsPtr<Component> components;
	ListOfAssetsPtr<Value> localVariables;

	// List of all component inputs of all components in this flow
	// When flow state is created we reserve this many Value's in memory
	// to keep the latest value of component input.
	ListOfFundamentalType<ComponentInput> componentInputs;

	ListOfAssetsPtr<WidgetDataItem> widgetDataItems;
	ListOfAssetsPtr<WidgetActionItem> widgetActions;

    ListOfFundamentalType<uint8_t> userPropertiesAssignable;
};

struct FlowDefinition {
	ListOfAssetsPtr<Flow> flows;
	ListOfAssetsPtr<Value> constants;
	ListOfAssetsPtr<Value> globalVariables;
};

struct Language {
    AssetsPtr<const char> languageID;
    ListOfAssetsPtr<const char> translations;
};

////////////////////////////////////////////////////////////////////////////////

struct Assets {
    uint8_t projectMajorVersion;
	uint8_t projectMinorVersion;
    uint8_t assetsType;
    uint8_t external;

    uint32_t reserved;

    AssetsPtr<Settings> settings;
#if EEZ_OPTION_GUI
	ListOfAssetsPtr<gui::PageAsset> pages;
	ListOfAssetsPtr<gui::Style> styles;
	ListOfAssetsPtr<gui::FontData> fonts;
	ListOfAssetsPtr<gui::Bitmap> bitmaps;
#endif // EEZ_OPTION_GUI
	AssetsPtr<Colors> colorsDefinition;
	ListOfAssetsPtr<const char> actionNames;
	ListOfAssetsPtr<const char> variableNames;
	AssetsPtr<FlowDefinition> flowDefinition;
    ListOfAssetsPtr<Language> languages;
    AssetsPtr<void> metadata;
};

////////////////////////////////////////////////////////////////////////////////

bool decompressAssetsData(const uint8_t *assetsData, uint32_t assetsDataSize, Assets *decompressedAssets, uint32_t maxDecompressedAssetsSize, int *err);

////////////////////////////////////////////////////////////////////////////////

void loadMainAssets(const uint8_t *assets, uint32_t assetsSize);

////////////////////////////////////////////////////////////////////////////////

#if EEZ_OPTION_GUI
const gui::PageAsset *getPageAsset(int pageId);
const gui::PageAsset* getPageAsset(int pageId, gui::WidgetCursor& widgetCursor);
const gui::Style *getStyle(int styleID);
const gui::FontData *getFontData(int fontID);
const gui::Bitmap *getBitmap(int bitmapID);
const int getBitmapIdByName(const char *bitmapName);
#endif

int getThemesCount();
const char *getThemeName(int i);
uint32_t getThemeColorsCount(int themeIndex);
// the colors as stored, 32-bit or RGB565 (see Theme)
const void *getThemeColors(int themeIndex);
const void *getColors();

#if EEZ_OPTION_GUI
const char *getActionName(const gui::WidgetCursor &widgetCursor, int16_t actionId);
int16_t getDataIdFromName(const gui::WidgetCursor &widgetCursor, const char *name);
#endif

////////////////////////////////////////////////////////////////////////////////

} // namespace eez
