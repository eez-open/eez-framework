/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <string.h>
#include <algorithm>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>

#include <eez/core/util.h>
#include <eez/core/utf8.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

bool styleIsHorzAlignLeft(const Style *style) {
    return (style->flags & STYLE_FLAGS_HORZ_ALIGN_MASK) == STYLE_FLAGS_HORZ_ALIGN_LEFT;
}

bool styleIsHorzAlignRight(const Style *style) {
    return (style->flags & STYLE_FLAGS_HORZ_ALIGN_MASK) == STYLE_FLAGS_HORZ_ALIGN_RIGHT;
}

bool styleIsVertAlignTop(const Style *style) {
    return (style->flags & STYLE_FLAGS_VERT_ALIGN_MASK) == STYLE_FLAGS_VERT_ALIGN_TOP;
}

bool styleIsVertAlignBottom(const Style *style) {
    return (style->flags & STYLE_FLAGS_VERT_ALIGN_MASK) == STYLE_FLAGS_VERT_ALIGN_BOTTOM;
}

////////////////////////////////////////////////////////////////////////////////

// AGG renderer is expensive to construct (and allocates its buffers on first use), so reuse it
static display::AggDrawing &beginAggDrawing() {
    static display::AggDrawing aggDrawing;
    display::startPixelsDraw();
    display::aggInit(aggDrawing);
    return aggDrawing;
}

static void endAggDrawing() {
    display::endPixelsDraw();
}

void drawBorderAndBackground(int &x1, int &y1, int &x2, int &y2, const Style *style, uint16_t color, bool ignoreLuminocity) {
    const WidgetCursor& widgetCursor = g_widgetCursor;

	bool isTransparent = true;
	bool hasBorder = false;
	bool hasBorderRadius = false;

	if (style) {
		hasBorder = style->borderSizeTop > 0 || style->borderSizeRight > 0 || style->borderSizeBottom > 0 || style->borderSizeLeft > 0;

		hasBorderRadius =
			style->borderRadiusTLX > 0 || style->borderRadiusTLY > 0 || style->borderRadiusTRX > 0 || style->borderRadiusTRY > 0 ||
			style->borderRadiusBRX > 0 || style->borderRadiusBRY > 0 || style->borderRadiusBLX > 0 || style->borderRadiusBLY > 0;


		// TODO: @mvladic
		// hasBorderRadius = 0;

		if (color != TRANSPARENT_COLOR_INDEX && style && style->opacity == 255 && !(hasBorder && hasBorderRadius)) {
			// non-transparent color
			isTransparent = false;
		} else if (style->backgroundImage) {
			auto bitmap = getBitmap(style->backgroundImage);
			if (bitmap->bpp != 32) {
				// non-transparent bitmap
				isTransparent = false;
			}
		}
	}

	if (isTransparent && !widgetCursor.refreshed) {
        size_t startStackPointer;
		if (widgetCursor.backgroundStyleStackPointer > 0) {
			for (startStackPointer = widgetCursor.backgroundStyleStackPointer - 1; startStackPointer > 0; startStackPointer--) {
				auto &backgroundStyle = widgetCursor.backgroundStyleStack[startStackPointer];

				auto color = backgroundStyle.active ? backgroundStyle.style->activeBackgroundColor : backgroundStyle.style->backgroundColor;
				if (color != TRANSPARENT_COLOR_INDEX && backgroundStyle.style->opacity == 255) {
					// non-transparent color
					break;
				} else if (backgroundStyle.style->backgroundImage) {
					auto bitmap = getBitmap(backgroundStyle.style->backgroundImage);
					if (bitmap) {
						if (bitmap->bpp != 32) {
							// non-transparent bitmap
							break;
						}
					}
				}
			}
		} else {
			startStackPointer = 0;
		}

		bool hasOpaqueBackground = false;
		if (widgetCursor.backgroundStyleStackPointer > 0) {
			auto &backgroundStyle = widgetCursor.backgroundStyleStack[startStackPointer];
			auto color = backgroundStyle.active ? backgroundStyle.style->activeBackgroundColor : backgroundStyle.style->backgroundColor;
			if (color != TRANSPARENT_COLOR_INDEX && backgroundStyle.style->opacity == 255) {
				hasOpaqueBackground = true;
			} else if (backgroundStyle.style->backgroundImage) {
				auto bitmap = getBitmap(backgroundStyle.style->backgroundImage);
				hasOpaqueBackground = bitmap && bitmap->bpp != 32;
			}
		}
		if (!hasOpaqueBackground) {
			// e.g. page with transparent background, what is below the page is the background
			display::drawBufferBackground(x1, y1, x2, y2);
		}

		for (size_t i = startStackPointer; i < widgetCursor.backgroundStyleStackPointer; i++) {
			auto &backgroundStyle = widgetCursor.backgroundStyleStack[i];

			auto color = backgroundStyle.active ? backgroundStyle.style->activeBackgroundColor : backgroundStyle.style->backgroundColor;
			if (color != TRANSPARENT_COLOR_INDEX) {
				display::setColor(color, ignoreLuminocity);
				auto savedOpacity = display::setOpacity(backgroundStyle.style->opacity);
                if (x2 >= x1 && y2 >= y1) {
                    display::fillRect(x1, y1, x2, y2);
                }
				display::setOpacity(savedOpacity);
			}

			if (backgroundStyle.style->backgroundImage) {
				auto bitmap = getBitmap(backgroundStyle.style->backgroundImage);
				if (bitmap) {
                    int x = backgroundStyle.x;
                    int y = backgroundStyle.y;
                    int w = MIN(x2 - x1 + 1, x + bitmap->w - x1);
                    int h = MIN(y2 - y1 + 1, y + bitmap->h - y1);

                    if (w >= 0 && h > 0) {
                        auto bytesPerPixel = bitmap->bpp  / 8;

                        uint32_t offset = 0;

                        if (x < x1) {
                            offset += (x1 - x) * bytesPerPixel;
                        }

                        if (y < y1) {
                            offset += (y1 - y) * bitmap->w * bytesPerPixel;
                        }

                        Image image;

                        image.width = w;
                        image.height = h;
                        image.bpp = bitmap->bpp;
                        image.lineOffset = bitmap->w - w;
                        image.pixels = (uint8_t *)bitmap->pixels + offset;

                        auto savedOpacity = display::setOpacity(backgroundStyle.style->opacity);
                        display::drawBitmap(&image, x1, y1);
                        display::setOpacity(savedOpacity);
                    }
				}
			}
		}
	}

	if (!style) {
		return;
	}

    if (hasBorder) {
		if (style->borderColor != TRANSPARENT_COLOR_INDEX) {
			display::setColor(style->borderColor, ignoreLuminocity);
		}

        if (hasBorderRadius) {
			if (color != TRANSPARENT_COLOR_INDEX) {
				display::setBackColor(color, ignoreLuminocity);
			}

            int lineWidth = style->borderSizeTop;
            if (lineWidth < style->borderSizeRight) {
                lineWidth = style->borderSizeRight;
            }
            if (lineWidth < style->borderSizeBottom) {
                lineWidth = style->borderSizeBottom;
            }
            if (lineWidth < style->borderSizeLeft) {
                lineWidth = style->borderSizeLeft;
            }

            auto savedOpacity = display::setOpacity(style->opacity);
            auto &aggDrawing = beginAggDrawing();
            display::fillRoundedRect(
                aggDrawing,
				x1, y1, x2, y2,
				lineWidth,
				style->borderRadiusTLX, style->borderRadiusTLY, style->borderRadiusTRX, style->borderRadiusTRY,
				style->borderRadiusBRX, style->borderRadiusBRY, style->borderRadiusBLX, style->borderRadiusBLY,
				style->borderColor != TRANSPARENT_COLOR_INDEX, color != TRANSPARENT_COLOR_INDEX
			);
            endAggDrawing();
            display::setOpacity(savedOpacity);

            lineWidth++;

            x1 += lineWidth;
            y1 += lineWidth;
            x2 -= lineWidth;
            y2 -= lineWidth;

            return;
        }

		if (style->borderColor != TRANSPARENT_COLOR_INDEX) {
			if (style->borderSizeLeft > 0) {
				display::fillRect(x1, y1, x1 + style->borderSizeLeft - 1, y2);
			}
			if (style->borderSizeTop > 0) {
				display::fillRect(x1, y1, x2, y1 + style->borderSizeTop - 1);
			}
			if (style->borderSizeRight > 0) {
				display::fillRect(x2 - (style->borderSizeRight - 1), y1, x2, y2);
			}
			if (style->borderSizeBottom > 0) {
				display::fillRect(x1, y2 - (style->borderSizeBottom - 1), x2, y2);
			}
		}

        x1 += style->borderSizeLeft;
        y1 += style->borderSizeTop;
        x2 -= style->borderSizeRight;
        y2 -= style->borderSizeBottom;
	} else {
		if (hasBorderRadius) {
			if (color != TRANSPARENT_COLOR_INDEX) {
				display::setBackColor(color, ignoreLuminocity);
		        auto savedOpacity = display::setOpacity(style->opacity);
                auto &aggDrawing = beginAggDrawing();
                display::fillRoundedRect(
                    aggDrawing,
					x1, y1, x2, y2,
					0,
					style->borderRadiusTLX, style->borderRadiusTLY, style->borderRadiusTRX, style->borderRadiusTRY,
					style->borderRadiusBRX, style->borderRadiusBRY, style->borderRadiusBLX, style->borderRadiusBLY,
					style->borderColor != TRANSPARENT_COLOR_INDEX, color != TRANSPARENT_COLOR_INDEX
				);
                endAggDrawing();
		        display::setOpacity(savedOpacity);
			}
			return;
		}
	}

	if (color != TRANSPARENT_COLOR_INDEX) {
		display::setColor(color, ignoreLuminocity);
		auto savedOpacity = display::setOpacity(style->opacity);
        if (x2 >= x1 && y2 >= y1) {
		    display::fillRect(x1, y1, x2, y2);
        }
		display::setOpacity(savedOpacity);
	}

    if (style->backgroundImage) {
        auto bitmap = getBitmap(style->backgroundImage);
		if (bitmap) {
            Image image;

			int w = MIN(x2 - x1 + 1, bitmap->w);
			int h = MIN(y2 - y1 + 1, bitmap->h);

            image.width = w;
            image.height = h;
            image.bpp = bitmap->bpp;
			image.lineOffset = bitmap->w - w;
            image.pixels = (uint8_t *)bitmap->pixels;

            auto savedOpacity = display::setOpacity(style->opacity);
            display::drawBitmap(&image, x1, y1);
            display::setOpacity(savedOpacity);
        }
    }
}

void drawText(
    const char *text, int textLength,
    int x, int y, int w, int h,
    const Style *style,
    bool active, bool blink, bool ignoreLuminocity,
    uint16_t *overrideColor, uint16_t *overrideBackgroundColor,
    uint16_t *overrideActiveColor, uint16_t *overrideActiveBackgroundColor,
    bool useSmallerFontIfDoesNotFit, int cursorPosition, int xScroll,
	bool boolSkipBackground
) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

	if (!boolSkipBackground) {
		uint16_t backgroundColor;
		if (active || blink) {
			if (overrideActiveBackgroundColor) {
				backgroundColor = *overrideActiveBackgroundColor;
			} else {
				backgroundColor = style->activeBackgroundColor;
			}
		} else {
			if (overrideBackgroundColor) {
				backgroundColor = *overrideBackgroundColor;
			} else {
				backgroundColor = style->backgroundColor;
			}
		}
		drawBorderAndBackground(x1, y1, x2, y2, style, backgroundColor, ignoreLuminocity);
	}

    uint16_t color;

    if (active || blink) {
        if (overrideActiveColor) {
            color = *overrideActiveColor;
        } else {
            color = style->activeColor;
        }
    }  else {
        if (overrideColor) {
            color = *overrideColor;
        } else {
            color = style->color;
        }
    }

    if (color == TRANSPARENT_COLOR_INDEX) {
        return;
    }

    font::Font font = styleGetFont(style);
	if (!font) {
		return;
	}

    // text width is not needed for the left aligned text
    int width = 0;
    if (useSmallerFontIfDoesNotFit || !styleIsHorzAlignLeft(style)) {
        width = display::measureStr(text, textLength, font, 0);
        while (useSmallerFontIfDoesNotFit && width > x2 - x1 + 1 && g_hooks.styleGetSmallerFont(font)) {
            width = display::measureStr(text, textLength, font, 0);
        }
    }
    int height = font.getHeight();

    int x_offset;
    if (styleIsHorzAlignLeft(style)) {
        x_offset = x1 + style->paddingLeft;
    } else if (styleIsHorzAlignRight(style)) {
        x_offset = x2 - style->paddingRight - width;
    } else {
        x_offset = x1 + ((x2 - x1 + 1) - width) / 2;
        if (x_offset < x1) {
            x_offset = x1;
        }
    }

    int y_offset;
    if (styleIsVertAlignTop(style)) {
        y_offset = y1 + style->paddingTop;
    } else if (styleIsVertAlignBottom(style)) {
        y_offset = y2 - style->paddingBottom - height;
    } else {
        y_offset = y1 + ((y2 - y1 + 1) - height) / 2;
    }
    // box above the display (e.g. list item while touch scrolling) is not snapped
    if (y_offset < 0 && y1 >= 0) {
        y_offset = y1;
    }

    // draw text
    display::setColor(color, ignoreLuminocity);
    display::drawStr(text, textLength, x_offset - xScroll, y_offset, x1, y1, x2, y2, font, cursorPosition);
}

////////////////////////////////////////////////////////////////////////////////

int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int w, int h, const Style *style) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    x1 += style->borderSizeLeft;
    y1 += style->borderSizeTop;
    x2 -= style->borderSizeRight;
    y2 -= style->borderSizeBottom;

    font::Font font = styleGetFont(style);

    int width = display::measureStr(text, textLength, font, 0);
    int height = font.getHeight();

    int x_offset;
    if (styleIsHorzAlignLeft(style)) {
        x_offset = x1 + style->paddingLeft;
    } else if (styleIsHorzAlignRight(style)) {
        x_offset = x2 - style->paddingRight - width;
    } else {
        x_offset = x1 + ((x2 - x1 + 1) - width) / 2;
        if (x_offset < x1) {
            x_offset = x1;
        }
    }

    int y_offset;
    if (styleIsVertAlignTop(style)) {
        y_offset = y1 + style->paddingTop;
    } else if (styleIsVertAlignBottom(style)) {
        y_offset = y2 - style->paddingBottom - height;
    } else {
        y_offset = y1 + ((y2 - y1 + 1) - height) / 2;
    }
    // box above the display (e.g. list item while touch scrolling) is not snapped
    if (y_offset < 0 && y1 >= 0) {
        y_offset = y1;
    }

    return display::getCharIndexAtPosition(xPos, text, textLength, x_offset, y_offset, x1, y1, x2, y2, font);
}

int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int w, int h, const Style *style) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    x1 += style->borderSizeLeft;
    y1 += style->borderSizeTop;
    x2 -= style->borderSizeRight;
    y2 -= style->borderSizeBottom;

    font::Font font = styleGetFont(style);

    int width = display::measureStr(text, textLength, font, 0);
    int height = font.getHeight();

    int x_offset;
    if (styleIsHorzAlignLeft(style)) {
        x_offset = x1 + style->paddingLeft;
    } else if (styleIsHorzAlignRight(style)) {
        x_offset = x2 - style->paddingRight - width;
    } else {
        x_offset = x1 + ((x2 - x1 + 1) - width) / 2;
        if (x_offset < x1) {
            x_offset = x1;
        }
    }

    int y_offset;
    if (styleIsVertAlignTop(style)) {
        y_offset = y1 + style->paddingTop;
    } else if (styleIsVertAlignBottom(style)) {
        y_offset = y2 - style->paddingBottom - height;
    } else {
        y_offset = y1 + ((y2 - y1 + 1) - height) / 2;
    }
    // box above the display (e.g. list item while touch scrolling) is not snapped
    if (y_offset < 0 && y1 >= 0) {
        y_offset = y1;
    }

    return display::getCursorXPosition(cursorPosition, text, textLength, x_offset, y_offset, x1, y1, x2, y2, font);
}

////////////////////////////////////////////////////////////////////////////////

static const unsigned int CONF_MULTILINE_TEXT_MAX_LINE_LENGTH = 1000;

struct MultilineTextRender {
    const char *text;
    int x1;
    int y1;
    int x2;
    int y2;
    const Style *style;
    bool active;
    int firstLineIndent;
    int hangingIndent;

    font::Font font;
    int spaceWidth;

    int lineHeight;
    int textHeight;

    char line[CONF_MULTILINE_TEXT_MAX_LINE_LENGTH + 1];
    size_t lineLength;
    int lineIndent;
    int lineWidth;

    // font tags can span multiple words and lines, so track the active font
    // index (0 is the default font) at the start of the line and at the end of
    // the text added so far
    int lineStartFontIndex;
    int currentFontIndex;

    // lines laid out by layout(), when recording is enabled
    struct LineInfo {
        size_t offset; // in linesText
        int y;
        int width;
        int indent;
        int fontIndex;
    };
    std::vector<LineInfo> *lines = nullptr;
    std::vector<char> *linesText = nullptr;

    // number of code points in n bytes of UTF-8 text
    static int countCodepoints(const char *str, int n) {
        int count = 0;
        const char *end = str + n;
        while (str < end) {
            utf8_int32_t encoding;
            str = utf8codepoint(str, &encoding);
            ++count;
        }
        return count;
    }

    // width of n bytes of UTF-8 text, starting in the currentFontIndex font
    int measureBytes(const char *str, int n) {
        return display::measureStr(str, countCodepoints(str, n), font, 0, currentFontIndex);
    }

    void appendToLine(const char *str, size_t n) {
        for (size_t i = 0; i < n && lineLength < CONF_MULTILINE_TEXT_MAX_LINE_LENGTH; i++) {
            line[lineLength++] = str[i];
        }
        line[lineLength] = 0;
    }

    void flushLine(int y) {
        if (lineLength && lineWidth) {
            if (lines) {
                lines->push_back({ linesText->size(), y, lineWidth, lineIndent, lineStartFontIndex });
                linesText->insert(linesText->end(), line, line + lineLength + 1);
            }

            textHeight = MAX(textHeight, y + lineHeight - y1);

            lineLength = 0;
            line[0] = 0;
            lineWidth = lineIndent = hangingIndent;
        }
    }

    // breaks text into lines, returns text height
    int layout() {
        textHeight = 0;

        int y = y1;

        lineLength = 0;
        line[0] = 0;
        lineWidth = lineIndent = firstLineIndent;
        lineStartFontIndex = currentFontIndex = 0;

        int i = 0;
        while (true) {
            int j = i;
            while (text[i] != 0 && text[i] != ' ' && text[i] != '\n')
                ++i;

            int width = measureBytes(text + j, i - j);

            while (lineWidth + (lineLength ? spaceWidth : 0) + width > x2 - x1 + 1) {
				if (!lineLength) {
                    // word doesn't fit even into the empty line, cut off the last character
                    if (i <= j) {
                        break;
                    }
                    int k = i;
                    do {
                        --k;
                    } while (k > j && (text[k] & 0xC0) == 0x80);
                    if (k == j) {
                        // keep at least one character, even if it doesn't fit
                        break;
                    }
                    i = k;
                    width = measureBytes(text + j, i - j);
					continue;
				}

                flushLine(y);

                y += lineHeight;
                if (y + lineHeight - 1 > y2) {
                    break;
                }
            }

            if (y + lineHeight - 1 > y2) {
                break;
            }

            if (lineLength) {
                appendToLine(" ", 1);
                lineWidth += spaceWidth;
            }
            if (!lineLength) {
                lineStartFontIndex = currentFontIndex;
            }
            appendToLine(text + j, i - j);
            lineWidth += width;
            currentFontIndex = display::getFontIndexAtEnd(text + j, countCodepoints(text + j, i - j), currentFontIndex);

            while (text[i] == ' ') {
                ++i;
            }

            if (text[i] == 0 || text[i] == '\n') {
                flushLine(y);

                y += lineHeight;

                if (text[i] == 0) {
                    break;
                }

                ++i;

                int extraHeightBetweenParagraphs = (int)(0.2 * lineHeight);
                y += extraHeightBetweenParagraphs;

                if (y + lineHeight - 1 > y2) {
                    break;
                }
            }
        }

        flushLine(y);

        return textHeight + font.getHeight() - lineHeight;
    }

    bool initFont() {
        font = styleGetFont(style);

        lineHeight = (int)(0.9 * font.getHeight());
        if (lineHeight <= 0) {
            return false;
        }

        auto spaceGlyph = font.getGlyph(' ');
        spaceWidth = spaceGlyph ? spaceGlyph->dx : 0;

        return true;
    }

    int measure() {
        x1 += style->borderSizeLeft;
        y1 += style->borderSizeTop;
        x2 -= style->borderSizeRight;
        y2 -= style->borderSizeBottom;

        if (!initFont()) {
            return 0;
        }

        x1 += style->paddingLeft;
        x2 -= style->paddingRight;
        y1 += style->paddingTop;
        y2 -= style->paddingBottom;

        return layout();
    }

    void render() {
        drawBorderAndBackground(x1, y1, x2, y2, style, active ? style->activeBackgroundColor : style->backgroundColor);

        if (!initFont()) {
            return;
        }

        uint16_t color = active ? style->activeColor : style->color;
        if (color == TRANSPARENT_COLOR_INDEX) {
            return;
        }

        x1 += style->paddingLeft;
        x2 -= style->paddingRight;
        y1 += style->paddingTop;
        y2 -= style->paddingBottom;

        // layout once, remember the lines and draw them after vertical alignment is known
        static std::vector<LineInfo> s_lines;
        static std::vector<char> s_linesText;
        s_lines.clear();
        s_linesText.clear();
        lines = &s_lines;
        linesText = &s_linesText;

        int textHeight = layout();

        int dy;
        if (styleIsVertAlignTop(style)) {
            dy = 0;
        } else if (styleIsVertAlignBottom(style)) {
            dy = y2 + 1 - textHeight - y1;
        } else {
            dy = (int)((y2 - y1 + 1 - textHeight) / 2);
        }

        display::setColor(color);

        for (auto &lineInfo : s_lines) {
            int x;
            if (styleIsHorzAlignLeft(style)) {
                x = x1;
            } else if (styleIsHorzAlignRight(style)) {
                x = x2 + 1 - lineInfo.width;
            } else {
                x = x1 + int((x2 - x1 + 1 - lineInfo.width) / 2);
            }

            int y = lineInfo.y + dy;
            display::drawStr(s_linesText.data() + lineInfo.offset, -1, x + lineInfo.indent, y, x1, y1, x2, y2, font, -1, lineInfo.fontIndex);
        }
    }
};

void drawMultilineText(const char *text, int x, int y, int w, int h, const Style *style, bool active, bool blinking, int firstLineIndent, int hangingIndent) {
    MultilineTextRender multilineTextRender;

    multilineTextRender.text = text;
    multilineTextRender.x1 = x;
    multilineTextRender.y1 = y;
    multilineTextRender.x2 = x + w - 1;
    multilineTextRender.y2 = y + h - 1;
    multilineTextRender.style = style;
    multilineTextRender.active = active || blinking;
    multilineTextRender.firstLineIndent = firstLineIndent;
    multilineTextRender.hangingIndent = hangingIndent;

    multilineTextRender.render();
}

int measureMultilineText(const char *text, int x, int y, int w, int h, const Style *style, int firstLineIndent, int hangingIndent) {
    MultilineTextRender multilineTextRender;

    multilineTextRender.text = text;
    multilineTextRender.x1 = x;
    multilineTextRender.y1 = y;
    multilineTextRender.x2 = x + w - 1;
    multilineTextRender.y2 = y + h - 1;
    multilineTextRender.style = style;
    multilineTextRender.active = false;
    multilineTextRender.firstLineIndent = firstLineIndent;
    multilineTextRender.hangingIndent = hangingIndent;

    return multilineTextRender.measure();
}

////////////////////////////////////////////////////////////////////////////////

void drawBitmap(Image *image, int x, int y, int w, int h, const Style *style, bool active) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    int width = image->width;
    int height = image->height;

    int x_offset;
    if (styleIsHorzAlignLeft(style))
        x_offset = x1 + style->paddingLeft;
    else if (styleIsHorzAlignRight(style))
        x_offset = x2 - style->paddingRight - width;
    else
        x_offset = x1 + ((x2 - x1) - width) / 2;
    if (x_offset < 0 && x1 >= 0)
        x_offset = x1;

    int y_offset;
    if (styleIsVertAlignTop(style))
        y_offset = y1 + style->paddingTop;
    else if (styleIsVertAlignBottom(style))
        y_offset = y2 - style->paddingBottom - height;
    else
        y_offset = y1 + ((y2 - y1) - height) / 2;
    if (y_offset < 0 && y1 >= 0)
        y_offset = y1;

    // draw background
    if (w > width || h > height) {
        drawRectangle(x, y, w, h, style, active);
    }

    // draw bitmap
    uint8_t savedOpacity = display::getOpacity();

    if (active) {
        display::setBackColor(style->activeBackgroundColor);
        display::setColor(style->activeColor);
        display::setOpacity(style->opacity);
    } else {
        display::setBackColor(style->backgroundColor);
        display::setColor(style->color);
        display::setOpacity(style->opacity);
    }

    display::drawBitmap(image, x_offset, y_offset);

    display::setOpacity(savedOpacity);
}

////////////////////////////////////////////////////////////////////////////////

void drawRectangle(int x, int y, int w, int h, const Style *style, bool active, bool ignoreLuminocity, bool invertColors) {
    if (w > 0 && h > 0) {
        int x1 = x;
        int y1 = y;
        int x2 = x + w - 1;
        int y2 = y + h - 1;

        uint16_t color;
        if (style) {
            if (invertColors) {
                color = active ? style->activeBackgroundColor : style->backgroundColor;
            } else {
                color = active ? style->activeColor : style->color;
            }
        } else {
            color = TRANSPARENT_COLOR_INDEX;
        }

        drawBorderAndBackground(x1, y1, x2, y2, style, color, ignoreLuminocity);
    }
}

////////////////////////////////////////////////////////////////////////////////

enum ShadowGlpyh {
	SHADOW_GLYPH_TOP_LEFT,
	SHADOW_GLYPH_TOP,
	SHADOW_GLYPH_TOP_RIGHT,
	SHADOW_GLYPH_LEFT,
	SHADOW_GLYPH_RIGHT,
	SHADOW_GLYPH_BOTTOM_LEFT,
	SHADOW_GLYPH_BOTTOM,
	SHADOW_GLYPH_BOTTOM_RIGHT,
};

#ifndef SHADOW_GLYPH_TOP_PX
static const int T = 4;
static const int R = 7;
static const int B = 10;
static const int L = 7;

static const int W = 20;
static const int H = 20;
#else
static const int T = SHADOW_GLYPH_TOP_PX;
static const int R = SHADOW_GLYPH_RIGHT_PX;
static const int B = SHADOW_GLYPH_BOTTOM_PX;
static const int L = SHADOW_GLYPH_LEFT_PX;

static const int W = SHADOW_GLYPH_WIDTH_PX;
static const int H = SHADOW_GLYPH_HEIGHT_PX;
#endif

#ifndef CONF_SHADOW_OFFSET_X
#define CONF_SHADOW_OFFSET_X 3
#endif
#ifndef CONF_SHADOW_OFFSET_Y
#define CONF_SHADOW_OFFSET_Y 6
#endif
#ifndef CONF_SHADOW_BLUR
#define CONF_SHADOW_BLUR 16
#endif
#ifndef CONF_SHADOW_SPREAD
#define CONF_SHADOW_SPREAD -2
#endif
#ifndef CONF_SHADOW_CORNER_RADIUS
#define CONF_SHADOW_CORNER_RADIUS 3
#endif
#ifndef CONF_SHADOW_OPACITY
#define CONF_SHADOW_OPACITY 160
#endif

static ShadowParams g_shadowParams = {
    CONF_SHADOW_OFFSET_X, CONF_SHADOW_OFFSET_Y, CONF_SHADOW_BLUR, CONF_SHADOW_SPREAD, CONF_SHADOW_CORNER_RADIUS,
    0, 0, 0,
    CONF_SHADOW_OPACITY
};
static uint32_t g_shadowParamsVersion;

void setShadowParams(const ShadowParams &params) {
    g_shadowParams = params;
    g_shadowParamsVersion++;
}

const ShadowParams &getShadowParams() {
    return g_shadowParams;
}

#if EEZ_USE_SDL
#define EEZ_SW_SHADOW 1

static float shadowSigma() {
    return g_shadowParams.blur > 1 ? g_shadowParams.blur / 2.0f : 0.5f;
}

// how far the shadow reaches beyond the shadow box
static int shadowReach() {
    return (int)ceilf(3.0f * shadowSigma());
}

static void getShadowExtents(int &left, int &top, int &right, int &bottom) {
    int reach = shadowReach() + g_shadowParams.spread;
    left = std::max(reach - g_shadowParams.offsetX, 0);
    right = std::max(reach + g_shadowParams.offsetX, 0);
    top = std::max(reach - g_shadowParams.offsetY, 0);
    bottom = std::max(reach + g_shadowParams.offsetY, 0);
}

// fast erf approximation, max error ~5e-4
static inline float erfApprox(float x) {
    float s = x < 0 ? -1.0f : 1.0f;
    float a = fabsf(x);
    float t = 1.0f + (0.278393f + (0.230389f + 0.078108f * (a * a)) * a) * a;
    t *= t;
    return s - s / (t * t);
}

static inline float gaussian(float x, float sigma) {
    return expf(-(x * x) / (2.0f * sigma * sigma)) / (2.5066283f * sigma);
}

// horizontal integral of the rounded box at vertical distance y from its center
static inline float roundedBoxShadowX(float x, float y, float sigma, float corner, float halfW, float halfH) {
    float delta = std::min(halfH - corner - fabsf(y), 0.0f);
    float curved = halfW - corner + sqrtf(std::max(0.0f, corner * corner - delta * delta));
    float k = 0.70710678f / sigma;
    return 0.5f * (erfApprox((x + curved) * k) - erfApprox((x - curved) * k));
}

// gaussian blurred rounded box coverage at point (x, y) relative to the box center
// (https://madebyevan.com/shaders/fast-rounded-rectangle-shadows/)
static float roundedBoxShadow(float x, float y, float halfW, float halfH, float sigma, float corner) {
    float low = y - halfH;
    float high = y + halfH;
    float start = std::min(std::max(-3.0f * sigma, low), high);
    float end = std::min(std::max(3.0f * sigma, low), high);
    float step = (end - start) / 4.0f;
    float yy = start + step * 0.5f;
    float value = 0.0f;
    for (int i = 0; i < 4; i++) {
        value += roundedBoxShadowX(x, y - yy, sigma, corner, halfW, halfH) * gaussian(yy, sigma) * step;
        yy += step;
    }
    return value;
}

static void drawShadowSW(int x1, int y1, int x2, int y2) {
    // shadow box
    int bx1 = x1 - g_shadowParams.spread + g_shadowParams.offsetX;
    int by1 = y1 - g_shadowParams.spread + g_shadowParams.offsetY;
    int bw = x2 - x1 + 1 + 2 * g_shadowParams.spread;
    int bh = y2 - y1 + 1 + 2 * g_shadowParams.spread;
    if (bw <= 0 || bh <= 0) {
        return;
    }

    int reach = shadowReach();
    int mw = bw + 2 * reach;
    int mh = bh + 2 * reach;

    // alpha mask depends only on box size and params, so cache it
    // (a few masks, there can be more windows with shadow on the screen)
    struct ShadowMask {
        std::vector<uint8_t> data;
        int w = -1;
        int h = -1;
        uint32_t version = 0;
        uint32_t lastUsed = 0;
    };
    static const int NUM_CACHED_MASKS = 4;
    static ShadowMask masks[NUM_CACHED_MASKS];
    static uint32_t useCounter;

    ShadowMask *shadowMask = nullptr;
    for (int i = 0; i < NUM_CACHED_MASKS; i++) {
        if (masks[i].w == bw && masks[i].h == bh && masks[i].version == g_shadowParamsVersion) {
            shadowMask = &masks[i];
            break;
        }
    }

    if (!shadowMask) {
        // replace least recently used
        shadowMask = &masks[0];
        for (int i = 1; i < NUM_CACHED_MASKS; i++) {
            if (masks[i].lastUsed < shadowMask->lastUsed) {
                shadowMask = &masks[i];
            }
        }

        shadowMask->w = bw;
        shadowMask->h = bh;
        shadowMask->version = g_shadowParamsVersion;
        auto &mask = shadowMask->data;
        mask.resize(mw * mh);

        float sigma = shadowSigma();
        float halfW = bw / 2.0f;
        float halfH = bh / 2.0f;
        float corner = std::min((float)std::max(g_shadowParams.cornerRadius, 0), std::min(halfW, halfH));

        // deep inside the box (farther than corner + 3 sigma from the edges) the shadow is fully opaque
        int inner = reach + (int)ceilf(corner) + reach + 1;

        // the mask is symmetric, compute one quadrant and mirror it
        int qw = (mw + 1) / 2;
        int qh = (mh + 1) / 2;
        for (int my = 0; my < qh; my++) {
            float py = my + 0.5f - mh / 2.0f;
            for (int mx = 0; mx < qw; mx++) {
                uint8_t a;
                if (mx >= inner && my >= inner) {
                    a = 255;
                } else {
                    float px = mx + 0.5f - mw / 2.0f;
                    float v = roundedBoxShadow(px, py, halfW, halfH, sigma, corner);
                    a = (uint8_t)std::min(std::max((int)(v * 255.0f + 0.5f), 0), 255);
                }
                mask[my * mw + mx] = a;
                mask[my * mw + (mw - 1 - mx)] = a;
                mask[(mh - 1 - my) * mw + mx] = a;
                mask[(mh - 1 - my) * mw + (mw - 1 - mx)] = a;
            }
        }
    }

    shadowMask->lastUsed = ++useCounter;
    const auto &mask = shadowMask->data;

    int mx1 = bx1 - reach;
    int my1 = by1 - reach;

    int displayWidth = display::getDisplayWidth();

    // clip to the current clip rectangle (it is inside the display)
    auto &clipRect = display::getClipRect();
    int sx1 = std::max(mx1, clipRect.x1);
    int sy1 = std::max(my1, clipRect.y1);
    int sx2 = std::min(mx1 + mw, clipRect.x2 + 1);
    int sy2 = std::min(my1 + mh, clipRect.y2 + 1);

    const int sr = g_shadowParams.r;
    const int sg = g_shadowParams.g;
    const int sb = g_shadowParams.b;
    const int opacity = g_shadowParams.opacity;

    uint32_t *buffer = (uint32_t *)display::getBufferPointer();

    for (int y = sy1; y < sy2; y++) {
        const uint8_t *maskRow = mask.data() + (y - my1) * mw;
        uint32_t *dstRow = buffer + y * displayWidth;
        bool insideRows = y >= y1 && y <= y2;
        for (int x = sx1; x < sx2; x++) {
            // skip area covered by the window itself
            if (insideRows && x >= x1 && x <= x2) {
                x = x2;
                continue;
            }

            int a = maskRow[x - mx1] * opacity;
            if (a < 255) {
                continue;
            }
            a = (a + 128) >> 8;

            uint8_t *p = (uint8_t *)(dstRow + x);
            p[0] = (uint8_t)(p[0] + (((sr - p[0]) * a) >> 8));
            p[1] = (uint8_t)(p[1] + (((sg - p[1]) * a) >> 8));
            p[2] = (uint8_t)(p[2] + (((sb - p[2]) * a) >> 8));
        }
    }
}
#endif

void drawShadowGlyph(ShadowGlpyh shadowGlyph, int x, int y, int xClip = -1, int yClip = -1) {
	font::Font font(getFontData(EEZ_FONT_ID_SHADOW));

	if (xClip == -1) {
		xClip = x + W - 1;
	}
	if (yClip == -1) {
		yClip = y + H - 1;
	}
    char glyph = 32 + shadowGlyph;
    
	display::drawStr(&glyph, 1, x, y, x, y, xClip, yClip, font, -1);
}

void drawShadow(int x1, int y1, int x2, int y2) {
#if defined(EEZ_SW_SHADOW)
    drawShadowSW(x1, y1, x2, y2);
#else
	display::setColor(64, 64, 64);

	int left = x1 - L;
	int top = y1 - T;

	int right = x2 + R - (W - 1);
	int bottom = y2 + B - (H - 1);

	drawShadowGlyph(SHADOW_GLYPH_TOP_LEFT, left, top);
	for (int x = left + W; x < right; x += W) {
		drawShadowGlyph(SHADOW_GLYPH_TOP, x, top, right - 1);
	}
	drawShadowGlyph(SHADOW_GLYPH_TOP_RIGHT, right, top);
	for (int y = top + H; y < bottom; y += H) {
		drawShadowGlyph(SHADOW_GLYPH_LEFT, left, y, -1, bottom - 1);
	}
	for (int y = top + H; y < bottom; y += H) {
		drawShadowGlyph(SHADOW_GLYPH_RIGHT, right, y, -1, bottom - 1);
	}
	drawShadowGlyph(SHADOW_GLYPH_BOTTOM_LEFT, left, bottom);
	for (int x = left + W; x < right; x += W) {
		drawShadowGlyph(SHADOW_GLYPH_BOTTOM, x, bottom, right - 1);
	}
	drawShadowGlyph(SHADOW_GLYPH_BOTTOM_RIGHT, right, bottom);
#endif
}

void expandRectWithShadow(int &x1, int &y1, int &x2, int &y2) {
#if defined(EEZ_SW_SHADOW)
    int left, top, right, bottom;
    getShadowExtents(left, top, right, bottom);
    x1 -= left;
    y1 -= top;
    x2 += right;
    y2 += bottom;
#else
	x1 -= L;
	y1 -= T;
	x2 += R;
	y2 += B;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void drawLine(int x1, int y1, int x2, int y2) {
    if (y1 == y2) {
        // horizontal
        display::fillRect(MIN(x1, x2), y1, MAX(x1, x2), y1);
        return;
    }

    if (x1 == x2) {
        // vertical
        display::fillRect(x1, MIN(y1, y2), x1, MAX(y1, y2));
        return;
    }

    display::startPixelsDraw();

    // Bresenham
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        display::drawPixel(x1, y1);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }

    display::endPixelsDraw();
}

// http://members.chello.at/~easyfilter/bresenham.html
void drawAntialiasedLine(int x0, int y0, int x1, int y1) {
    display::startPixelsDraw();

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx - dy, e2, x2;                       /* error value e_xy */
    int ed = dx + dy == 0 ? 1 : (int)sqrt((float)dx*dx + (float)dy*dy);

    for (; ; ) {                                         /* pixel loop */
        display::drawPixel(x0, y0, 255 - 255 * abs(err - dx + dy) / ed);
        e2 = err; x2 = x0;
        if (2 * e2 >= -dx) {                                    /* x step */
            if (x0 == x1) break;
            if (e2 + dy < ed) display::drawPixel(x0, y0 + sy, 255 - 255 * (e2 + dy) / ed);
            err -= dy; x0 += sx;
        }
        if (2 * e2 <= dy) {                                     /* y step */
            if (y0 == y1) break;
            if (dx - e2 < ed) display::drawPixel(x2 + sx, y0, 255 - 255 * (dx - e2) / ed);
            err += dx; y0 += sy;
        }
    }

    display::endPixelsDraw();
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
