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

#include <math.h>
#include <limits.h>

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/yt_graph.h>

#define CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR 10

namespace eez {
namespace gui {

// Pixels of the graph drawn with YT_GRAPH_UPDATE_METHOD_SCROLL and
// YT_GRAPH_UPDATE_METHOD_SCAN_LINE. Graph is drawn incrementally (only the new
// values) into this buffer and then the whole buffer is copied to the display,
// so the graph can be rendered again at any time, e.g. when it is overlapped by
// some other widget (see doStaticLayoutWithOverlapping).
struct YTGraphCanvas {
    display::Color *pixels;
    int w;
    int h;
    display::Color color;

    void setColor(display::Color color_) {
        color = color_;
    }

    void drawPixel(int x, int y) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            pixels[y * w + x] = color;
        }
    }

    void fillRect(int x1, int y1, int x2, int y2) {
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > w - 1) x2 = w - 1;
        if (y2 > h - 1) y2 = h - 1;
        for (int y = y1; y <= y2; y++) {
            auto p = pixels + y * w + x1;
            for (int x = x1; x <= x2; x++) {
                *p++ = color;
            }
        }
    }

    void drawVLine(int x, int y, int l) {
        fillRect(x, y, x, y + l);
    }

    // moves all the pixels n columns to the left
    void scrollLeft(int n) {
        if (n <= 0 || n >= w) {
            return;
        }
        for (int y = 0; y < h; y++) {
            auto row = pixels + y * w;
            memmove(row, row + n, (w - n) * sizeof(display::Color));
        }
    }
};

// used for YT_GRAPH_UPDATE_METHOD_SCROLL and YT_GRAPH_UPDATE_METHOD_SCAN_LINE
struct YTGraphDrawHelper {
    const WidgetCursor &widgetCursor;
    const Widget *widget;
    YTGraphCanvas &canvas;

    float min[2];
    float max[2];

    display::Color color;
    display::Color dataColor[2];

    uint32_t numPositions;
    uint32_t position;

    int x;

    int yPrev[2];
    int y[2];

    Value::YtDataGetValueFunctionPointer ytDataGetValue;

    YTGraphDrawHelper(const WidgetCursor &widgetCursor_, YTGraphCanvas &canvas_) : widgetCursor(widgetCursor_), widget(widgetCursor.widget), canvas(canvas_) {
        min[0] = ytDataGetMin(widgetCursor, widget->data, 0).getFloat();
        max[0] = ytDataGetMax(widgetCursor, widget->data, 0).getFloat();

        min[1] = ytDataGetMin(widgetCursor, widget->data, 1).getFloat();
        max[1] = ytDataGetMax(widgetCursor, widget->data, 1).getFloat();

        const Style* y1Style = ytDataGetStyle(widgetCursor, widget->data, 0);
        const Style* y2Style = ytDataGetStyle(widgetCursor, widget->data, 1);
        dataColor[0] = display::getColorFromIndex(y1Style->color);
        dataColor[1] = display::getColorFromIndex(y2Style->color);

        ytDataGetValue = ytDataGetGetValueFunc(widgetCursor, widget->data);
    }

    int getYValue(int valueIndex, uint32_t position) {
        if (position >= numPositions) {
            return INT_MIN;
        }

        float value = ytDataGetValue(position, valueIndex, nullptr);

        if (isNaN(value)) {
            return INT_MIN;
        }

        int y = (int)round((canvas.h - 1) * (value - min[valueIndex]) / (max[valueIndex] - min[valueIndex]));
        return canvas.h - 1 - y;
    }

    void drawValue(int valueIndex) {
		int yPrevValue = yPrev[valueIndex];
		int yValue = y[valueIndex];

		if (yValue == INT_MIN) {
            return;
        }

		// clipping
		if (yPrevValue >= 0 && yValue < 0) {
            yValue = 0;
        } else if (yPrevValue < 0 && yValue >= 0) {
            yPrevValue = 0;
        } else if (yPrevValue < canvas.h && yValue >= canvas.h) {
            yValue = canvas.h - 1;
        } else if (yPrevValue >= canvas.h && yValue < canvas.h) {
            yPrevValue = canvas.h - 1;
        }

		if (yValue < 0 || yPrevValue < 0 || yValue >= canvas.h || yPrevValue >= canvas.h) {
			return;
		}

        canvas.setColor(dataColor[valueIndex]);

        if (yPrevValue == INT_MIN || abs(yPrevValue - yValue) <= 1) {
            canvas.drawPixel(x, yValue);
        } else {
            int y1;
            int y2;
            if (yPrevValue < yValue) {
                y1 = yPrevValue + 1;
                y2 = y1 + yValue - yPrevValue;
            } else {
                y1 = yValue;
                y2 = y1 + yPrevValue - yValue;
            }
            for (int y = y1; y < y2; y++) {
                canvas.drawPixel(x, y);
            }
        }
    }

    void drawStep() {
        if (y[0] != INT_MIN && y[1] != INT_MIN && abs(yPrev[0] - y[0]) <= 1 && abs(yPrev[1] - y[1]) <= 1 && y[0] == y[1]) {
			if (y[0] >= 0 && y[0] < canvas.h) {
				canvas.setColor(position % 2 ? dataColor[1] : dataColor[0]);
				canvas.drawPixel(x, y[0]);
			}
        } else {
            drawValue(0);
            drawValue(1);
        }
    }

    void drawScanLine(uint32_t startPosition, uint32_t endPosition, uint16_t graphWidth) {
        numPositions = endPosition;

        int x1 = startPosition % graphWidth;
        int x2 = (endPosition - 1) % graphWidth;
        canvas.setColor(color);
        if (x1 <= x2) {
            canvas.fillRect(x1, 0, x2, canvas.h - 1);
        } else {
            canvas.fillRect(x1, 0, canvas.w - 1, canvas.h - 1);
            canvas.fillRect(0, 0, x2, canvas.h - 1);
        }

        for (position = startPosition; position < endPosition; ++position) {
            x = position % graphWidth;

            y[0] = getYValue(0, position);
            yPrev[0] = getYValue(0, position == 0 ? position : position - 1);

            y[1] = getYValue(1, position);
            yPrev[1] = getYValue(1, position == 0 ? position : position - 1);

            drawStep();
        }
    }

    void drawScrolling(uint32_t previousHistoryValuePosition, uint32_t currentHistoryValuePosition, uint32_t numPositions_, uint16_t graphWidth) {
        uint32_t numPointsToDraw = currentHistoryValuePosition - previousHistoryValuePosition;
        if (numPointsToDraw > graphWidth) {
            numPointsToDraw = graphWidth;
        }

        if (numPointsToDraw < graphWidth) {
            canvas.scrollLeft(numPointsToDraw);
        }

        int endX = graphWidth;
        int startX = endX - numPointsToDraw;

        position = previousHistoryValuePosition + 1;

        numPositions = position + numPointsToDraw;

        yPrev[0] = getYValue(0, previousHistoryValuePosition);
        yPrev[1] = getYValue(1, previousHistoryValuePosition);

        canvas.setColor(color);
        canvas.fillRect(startX, 0, endX - 1, canvas.h - 1);

        for (x = startX; x < endX; x++, position++) {
            y[0] = getYValue(0, position);
            y[1] = getYValue(1, position);

            drawStep();

            yPrev[0] = y[0];
            yPrev[1] = y[1];
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

// used for YT_GRAPH_UPDATE_METHOD_STATIC
struct YTGraphStaticDrawHelper {
    YTGraphWidgetState *widgetState;
    const WidgetCursor &widgetCursor;
    const Widget *widget;

    Style* style;

    display::Color dataColor;

    uint32_t numPositions;
    uint32_t position;

    float offset;
    float scale;

    int m_valueIndex;

    int x;

    int yPrevMin;
    int yPrevMax;
    int yMin;
    int yMax;

    Value::YtDataGetValueFunctionPointer ytDataGetValue;

    int xLabels[MAX_NUM_OF_Y_VALUES];
    int yLabels[MAX_NUM_OF_Y_VALUES];

    YTGraphStaticDrawHelper(YTGraphWidgetState *widgetState_, const WidgetCursor &widgetCursor_)
        : widgetState(widgetState_), widgetCursor(widgetCursor_), widget(widgetCursor.widget)
    {
        ytDataGetValue = ytDataGetGetValueFunc(widgetCursor, widget->data);
    }

    void getYValue(uint32_t position, int &min, int &max) {
        if (position >= numPositions) {
            max = INT_MIN;
            min = INT_MIN;
        } else {
            float fMax;
            float fMin = ytDataGetValue(position, m_valueIndex, &fMax);

            if (isNaN(fMin)) {
                max = INT_MIN;
            } else {
                max = widgetCursor.h - 1 - (int)floor(widgetCursor.h / 2.0f + (fMin + offset) * scale);
            }

            if (isNaN(fMax)) {
                min = INT_MIN;
            } else {
                min = widgetCursor.h - 1 - (int)floor(widgetCursor.h / 2.0f + (fMax + offset) * scale);
            }
        }
    }

    void drawValue() {
        if (yMin == INT_MIN) {
            return;
        }

        display::setColorByValue(dataColor);

        int yFrom;
        int yTo;

        if (yPrevMax == INT_MIN) {
            yFrom = yMin;
            yTo = yMax;
        } else {
            if (yPrevMax < yMin) {
                yFrom = yPrevMax + 1;
                yTo = yMax;
            } else if (yMax < yPrevMin) {
                yFrom = yMin;
                yTo = yPrevMin - 1;
            } else {
                yFrom = yMin;
                yTo = yMax;
            }
        }

        if ((yFrom < 0 && yTo < 0) || (yFrom >= widgetCursor.h && yTo >= widgetCursor.h)) {
            return;
        }

        if (yFrom < 0) {
            yFrom = 0;
        }

        if (yTo >= widgetCursor.h) {
            yTo = widgetCursor.h - 1;
        }

        if (yFrom == yTo) {
            display::drawPixel(x, widgetCursor.y + yFrom);
        } else {
            // display::drawVLine(x, widgetCursor.y + yFrom, yTo - yFrom);
            int y1 = widgetCursor.y + yFrom;
            int y2 = y1 + yTo - yFrom + 1;
            for (int y = y1; y < y2; y++) {
                display::drawPixel(x, y);
            }
        }
    }

    void getMinMax(int *yLabels, int n, int &yMin, int &yMax) {
        yMin = INT_MAX;
        yMax = INT_MIN;
        for (int i = 0; i < n; i++) {
            if (yLabels[i] < yMin) {
                yMin = yLabels[i];
            }
            if (yLabels[i] > yMax) {
                yMax = yLabels[i];
            }
        }
    }

    void repositionLabels(int labelHeight) {
        for (int valueIndex = 0; valueIndex < MAX_NUM_OF_Y_VALUES; valueIndex++) {
            if (yLabels[valueIndex] != INT_MIN) {
                yLabels[valueIndex] -= labelHeight / 2;
                if (yLabels[valueIndex] < widgetCursor.y) {
                    yLabels[valueIndex] = widgetCursor.y;
                } else if (yLabels[valueIndex] > widgetCursor.y + widgetCursor.h - labelHeight) {
                    yLabels[valueIndex] = widgetCursor.y + widgetCursor.h - labelHeight;
                }
            }
        }
    }

    void drawGraph(uint32_t currentHistoryValuePosition, int startX, int endX, int vertDivisions) {
        //xLabels[m_valueIndex] = INT_MIN;
        yLabels[m_valueIndex] = INT_MIN;

        if (ytDataDataValueIsVisible(widgetCursor, widget->data, m_valueIndex)) {
            position = currentHistoryValuePosition;

            scale = (widgetCursor.h - 1) / widgetState->valueDiv[m_valueIndex] / vertDivisions;
            offset = widgetState->valueOffset[m_valueIndex];

            const Style* style = ytDataGetStyle(widgetCursor, widget->data, m_valueIndex);
            dataColor = display::getColorFromIndex(style->color);

            getYValue(position > 0 ? position - 1 : 0, yPrevMin, yPrevMax);

            for (x = startX; x < endX; x++, position++) {
                getYValue(position, yMin, yMax);
                drawValue();
                yPrevMin = yMin;
                yPrevMax = yMax;

                if (yMin != INT_MIN) {
                    //xLabels[m_valueIndex] = x;
                    yLabels[m_valueIndex] = widgetCursor.y + yMin;
                }
            }
        }

		xLabels[m_valueIndex] = endX;
    }

    void drawLabel(font::Font &font, bool transparent) {
        if (yLabels[m_valueIndex] != INT_MIN) {
            const Style *labelStyle = ytDataGetStyle(widgetCursor, widget->data, m_valueIndex);

            char labelText[64];
            ytDataGetLabel(widgetCursor, widget->data, m_valueIndex, labelText, sizeof(labelText));
            int labelWidth = display::measureStr(labelText, -1, font, widgetCursor.w);

            int xLabel = xLabels[m_valueIndex];
            if (xLabel < widgetCursor.x) {
                xLabel = widgetCursor.x;
            } else if (xLabel > widgetCursor.x + widgetCursor.w - labelWidth) {
                xLabel = widgetCursor.x + widgetCursor.w - labelWidth;
            }

            if (!transparent) {
                display::setColor(labelStyle->backgroundColor);
                display::fillRect(xLabel, yLabels[m_valueIndex], xLabel + labelWidth - 1, yLabels[m_valueIndex] + font.getHeight() - 1);
            }

            display::setColor(labelStyle->color);
            display::drawStr(labelText, -1, xLabel, yLabels[m_valueIndex], widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w - 1, widgetCursor.y + widgetCursor.h - 1, font, -1);
        }
    }

    void drawStatic(uint32_t previousHistoryValuePosition, uint32_t currentHistoryValuePosition, uint32_t numPositions_, uint16_t graphWidth, bool showLabels, int selectedValueIndex) {
        // draw background
        const Style* style = getStyle(widget->style);
        display::setColor(style->backgroundColor);
        display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + (int)widgetCursor.w - 1, widgetCursor.y + (int)widgetCursor.h - 1);

        numPositions = numPositions_;

        int startX = widgetCursor.x;
        int endX = startX + graphWidth;

        int horzDivisions = ytDataGetHorzDivisions(widgetCursor, widget->data);
        int vertDivisions = ytDataGetVertDivisions(widgetCursor, widget->data);

        // draw grid
        display::setColor(style->borderColor);
        for (int x = 1; x < horzDivisions; x++) {
            display::drawVLine(widgetCursor.x + x * widgetCursor.w / horzDivisions, widgetCursor.y, widgetCursor.h - 1);
        }
        for (int y = 1; y < vertDivisions; y++) {
            display::drawHLine(widgetCursor.x, widgetCursor.y + y * widgetCursor.h / vertDivisions, widgetCursor.w - 1);
        }

		// draw bookmarks
		if (widgetState->bookmarks) {
			for (int x = 0; x < widgetCursor.w; x++) {
				if (widgetState->bookmarks[x]) {
					display::setColor(EEZ_COLOR_ID_BOOKMARK);
					display::drawVLine(startX + x, widgetCursor.y, widgetCursor.h - 1);
				}
			}
		}

        // draw graphs
        display::startPixelsDraw();
        for (m_valueIndex = 0; m_valueIndex < MAX_NUM_OF_Y_VALUES; m_valueIndex++) {
            if (m_valueIndex != selectedValueIndex) {
                drawGraph(currentHistoryValuePosition, startX, endX, vertDivisions);
            }
        }
        if (selectedValueIndex != -1) {
            m_valueIndex = selectedValueIndex;
            drawGraph(currentHistoryValuePosition, startX, endX, vertDivisions);
        }
        display::endPixelsDraw();

		// draw cursor
		if (ytDataIsCursorVisible(widgetCursor, widgetCursor.widget->data)) {
			display::setColor(style->color);
			display::drawVLine(startX + widgetState->cursorPosition - currentHistoryValuePosition, widgetCursor.y, widgetCursor.h - 1);

			char text[64];
			ytDataGetCursorXValue(widgetCursor, widgetCursor.widget->data).toText(text, sizeof(text));

			font::Font font = styleGetFont(style);
			int MIN_CURSOR_TEXT_WIDTH = 80;
			int cursorTextWidth = MAX(display::measureStr(text, -1, font), MIN_CURSOR_TEXT_WIDTH);
			int cursorTextHeight = font.getHeight();
			const int PADDING = 0;
			int xCursorText = widgetCursor.x + widgetState->cursorPosition - currentHistoryValuePosition - cursorTextWidth / 2;
			if (xCursorText < widgetCursor.x + PADDING) {
				xCursorText = widgetCursor.x + PADDING;
			} else if (xCursorText + cursorTextWidth > widgetCursor.x + widgetCursor.w - PADDING) {
				xCursorText = widgetCursor.x + widgetCursor.w - PADDING - cursorTextWidth;
			}
			int yCursorText = widgetCursor.y + widgetCursor.h - cursorTextHeight - PADDING;

			drawText(
                text, -1,
				xCursorText, yCursorText, cursorTextWidth, cursorTextHeight,
				style,
                widgetState->flags.focused
			);
		}

		// draw labels
        if (showLabels) {
            font::Font font = styleGetFont(style);
            int labelHeight = font.getHeight();

            repositionLabels(labelHeight);

            for (m_valueIndex = 0; m_valueIndex < MAX_NUM_OF_Y_VALUES; m_valueIndex++) {
                if (m_valueIndex != selectedValueIndex) {
                    drawLabel(font, true);
                }
            }

            if (selectedValueIndex != -1) {
                m_valueIndex = selectedValueIndex;
                drawLabel(font, false);
            }
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

YTGraphWidgetState::~YTGraphWidgetState() {
    eez::free(buffer);
}

bool YTGraphWidgetState::updateState() {
    WIDGET_STATE_START(YTGraphWidget);

    refreshBackground = !hasPreviousState;

    WIDGET_STATE(flags.focused, isFocusWidget(widgetCursor));

    WIDGET_STATE(refreshCounter, ytDataGetRefreshCounter(widgetCursor, widget->data));
    WIDGET_STATE(iChannel, widgetCursor.cursor);
    WIDGET_STATE(ytGraphUpdateMethod, ytDataGetGraphUpdateMethod(widgetCursor, widget->data));

	auto new_historyValuePosition = ytDataGetPosition(widgetCursor, widget->data);

    if (hasPreviousState) {
    	previousHistoryValuePosition = historyValuePosition;
    } else {
        uint16_t graphWidth = (uint16_t)widgetCursor.w;
    	previousHistoryValuePosition = new_historyValuePosition - graphWidth;
        hasPreviousState = false;
        redrawAll = true;
    }

    WIDGET_STATE(historyValuePosition, new_historyValuePosition);
    WIDGET_STATE(cursorPosition, historyValuePosition + ytDataGetCursorOffset(widgetCursor, widget->data));

    WIDGET_STATE(numHistoryValues, ytDataGetSize(widgetCursor, widget->data));
    WIDGET_STATE(bookmarks, ytDataGetBookmarks(widgetCursor, widget->data));
    WIDGET_STATE(showLabels, ytDataGetShowLabels(widgetCursor, widget->data));
    WIDGET_STATE(selectedValueIndex, ytDataGetSelectedValueIndex(widgetCursor, widget->data));

    if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_STATIC) {
        for (int valueIndex = 0; valueIndex < MAX_NUM_OF_Y_VALUES; valueIndex++) {
            WIDGET_STATE(valueIsVisible[valueIndex], ytDataDataValueIsVisible(widgetCursor, widget->data, valueIndex));
            WIDGET_STATE(valueDiv[valueIndex], ytDataGetDiv(widgetCursor, widget->data, valueIndex));
            WIDGET_STATE(valueOffset[valueIndex], ytDataGetOffset(widgetCursor, widget->data, valueIndex));
        }
    }

    WIDGET_STATE_END()
}

void YTGraphWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const YTGraphWidget *)widgetCursor.widget;

    uint16_t graphWidth = (uint16_t)widgetCursor.w;

    if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_STATIC) {
        YTGraphStaticDrawHelper drawHelper(this, widgetCursor);
        drawHelper.drawStatic(previousHistoryValuePosition, historyValuePosition, numHistoryValues, graphWidth, showLabels, selectedValueIndex);
    } else {
        const Style* style = getStyle(widget->style);

        int w = widgetCursor.w;
        int h = widgetCursor.h;
        if (w <= 0 || h <= 0) {
            return;
        }

        if (!buffer || bufferWidth != w || bufferHeight != h) {
            eez::free(buffer);
            buffer = eez::alloc(w * h * sizeof(display::Color), 0x3c5e81d7);
            if (!buffer) {
                bufferWidth = 0;
                bufferHeight = 0;
                return;
            }
            bufferWidth = w;
            bufferHeight = h;
            refreshBackground = true;
            redrawAll = true;
        }

        // Render can be called more than once for the same state (e.g. when
        // overlapped by other widget), so only the values that are not already
        // in the buffer are drawn.
        uint32_t startPosition;
        if (
            refreshBackground || redrawAll ||
            bufferHistoryValuePosition > historyValuePosition ||
            historyValuePosition - bufferHistoryValuePosition > graphWidth
        ) {
            // whole graph
            startPosition = historyValuePosition - graphWidth;
        } else {
            startPosition = bufferHistoryValuePosition;
        }

        if (refreshBackground || startPosition != historyValuePosition) {
            YTGraphCanvas canvas = { (display::Color *)buffer, w, h, 0 };

            // buffer could be still in use by the previous drawBitmap
            display::startPixelsDraw();

            if (refreshBackground) {
                canvas.setColor(display::getColorFromIndex(style->backgroundColor));
                canvas.fillRect(0, 0, w - 1, h - 1);
            }

            if (startPosition != historyValuePosition) {
                YTGraphDrawHelper drawHelper(widgetCursor, canvas);
                drawHelper.color = display::getColorFromIndex(flags.active ? style->color : style->backgroundColor);
                if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCAN_LINE) {
                    drawHelper.drawScanLine(startPosition, historyValuePosition, graphWidth);

                    // draw cursor
                    canvas.setColor(display::getColorFromIndex(style->color));
                    canvas.drawVLine(historyValuePosition % graphWidth, 0, h - 1);
                    canvas.drawVLine((historyValuePosition + 1) % graphWidth, 0, h - 1);

                    // draw blank lines
                    int x1 = (historyValuePosition + 2) % graphWidth;
                    int x2 = (historyValuePosition + CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR) % graphWidth;

                    canvas.setColor(display::getColorFromIndex(style->backgroundColor));
                    if (x1 < x2) {
                        canvas.fillRect(x1, 0, x2, h - 1);
                    } else {
                        canvas.fillRect(x1, 0, graphWidth - 1, h - 1);
                        canvas.fillRect(0, 0, x2, h - 1);
                    }
                } else if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCROLL) {
                    drawHelper.drawScrolling(startPosition, historyValuePosition, numHistoryValues, graphWidth);
                }
            }

            bufferHistoryValuePosition = historyValuePosition;
            refreshBackground = false;
            redrawAll = false;
        }

        Image image;
        image.width = w;
        image.height = h;
        image.bpp = DISPLAY_BPP;
        image.lineOffset = 0;
        image.pixels = (uint8_t *)buffer;
        display::drawBitmap(&image, widgetCursor.x, widgetCursor.y);
    }
}

bool YTGraphWidgetState::hasOnTouch() {
    return ytDataGetGraphUpdateMethod(g_widgetCursor, g_widgetCursor.widget->data) == YT_GRAPH_UPDATE_METHOD_STATIC;
}

void YTGraphWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN || touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        TouchDrag touchDrag = {
            widgetCursor,
            touchEvent.type,
            touchEvent.x - widgetCursor.x,
            touchEvent.y - widgetCursor.y
        };
        ytDataTouchDrag(widgetCursor, widgetCursor.widget->data, &touchDrag);
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
