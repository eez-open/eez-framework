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
#include <stdio.h>
#include <string.h>

#include <eez/core/os.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/bar_graph.h>

namespace eez {
namespace gui {

#define BAR_GRAPH_ORIENTATION_LEFT_RIGHT 1
#define BAR_GRAPH_ORIENTATION_RIGHT_LEFT 2
#define BAR_GRAPH_ORIENTATION_TOP_BOTTOM 3
#define BAR_GRAPH_ORIENTATION_BOTTOM_TOP 4
#define BAR_GRAPH_ORIENTATION_MASK 0x0F
#define BAR_GRAPH_DO_NOT_DISPLAY_VALUE (1 << 4)

int calcValuePosInBarGraphWidget(Value &value, float min, float max, int d) {
    return (int)roundf((value.toFloat() - min) * d / (max - min));
}

int calcValuePosInBarGraphWidgetWithClamp(Value &value, float min, float max, int d) {
    int p = calcValuePosInBarGraphWidget(value, min, max, d);

    if (p < 0) {
        p = 0;
    } else if (p >= d) {
        p = d - 1;
    }

    return p;
}

void drawLineInBarGraphWidget(const BarGraphWidget *barGraphWidget, int p, int16_t lineStyleID, int x, int y, int w, int h) {
    const Style *style = getStyle(lineStyleID);

    display::setColor(style->color);
    if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
        display::drawVLine(x + p, y, h - 1);
    } else if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_RIGHT_LEFT) {
        display::drawVLine(x - p, y, h - 1);
    } else if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
        display::drawHLine(x, y + p, w - 1);
    } else {
        display::drawHLine(x, y - p, w - 1);
    }
}

bool BarGraphWidgetState::updateState() {
    WIDGET_STATE_START(BarGraphWidget);

    WIDGET_STATE(styleId, overrideStyle(widgetCursor, widget->style));
    const Style* style = getStyle(styleId);

    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(blinkOpacity, isBlinking(widgetCursor, widget->data) ? g_blinkOpacity : 255);
    WIDGET_STATE(data, get(widgetCursor, widget->data));
    WIDGET_STATE(color, getColor(widgetCursor, widget->data, style));
    WIDGET_STATE(backgroundColor, getBackgroundColor(widgetCursor, widget->data, style));
    WIDGET_STATE(line1Data, get(widgetCursor, widget->line1Data));
    WIDGET_STATE(line2Data, get(widgetCursor, widget->line2Data));

    if (widgetCursor.flowState) {
        WIDGET_STATE(min, get(widgetCursor, widget->min));
        WIDGET_STATE(max, get(widgetCursor, widget->max));
        WIDGET_STATE(refreshRate, get(widgetCursor, widget->refreshRate));
        WIDGET_STATE(line1Label, widget->line1Label ? get(widgetCursor, widget->line1Label) : Value());
    }

    bool refreshTextData = true;
    auto currentTime = millis();
    if (!textData.isUndefinedOrNull() || textData != data) {
        uint32_t refreshRateMs = widgetCursor.flowState ? refreshRate.toInt32(nullptr) : getTextRefreshRate(widgetCursor, widget->data);
        if (refreshRateMs != 0 && currentTime - textDataRefreshLastTime < refreshRateMs) {
            refreshTextData = false;
        }
    }

    if (refreshTextData) {
        WIDGET_STATE(textData, data);
        textDataRefreshLastTime = currentTime;
    }

    WIDGET_STATE_END()
}

void BarGraphWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const BarGraphWidget *)widgetCursor.widget;
    const Style* style = getStyle(styleId);

    int x = widgetCursor.x;
    int y = widgetCursor.y;
    const int w = widgetCursor.w;
    const int h = widgetCursor.h;

    float min;
    float max;

    if (widgetCursor.flowState) {
        min = this->min.toFloat(nullptr);
        max = this->max.toFloat(nullptr);
    } else {
        min = getMin(widgetCursor, widget->data).getFloat();

        Value displayValueRange = getDisplayValueRange(widgetCursor, widget->data);
        if (displayValueRange.getType() == VALUE_TYPE_FLOAT) {
            max = displayValueRange.getFloat();
        } else {
            max = getMax(widgetCursor, widget->data).getFloat();
        }
    }

    if (widget->flags || widget->scaleStyle || widget->thickness) {
        renderTrack(widget, style, min, max);
        return;
    }

    bool horizontal =
        (widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT ||
        (widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_RIGHT_LEFT;

    int d = horizontal ? w : h;

    // calc bar position (monitored value)
    int pValue = calcValuePosInBarGraphWidgetWithClamp(data, min, max, d);

    // calc line 1 position (set value)
    int pLine1 = calcValuePosInBarGraphWidget(line1Data, min, max, d);
    bool drawLine1 = pLine1 >= 0 && pLine1 < d;

    // calc line 2 position (limit value)
    int pLine2 = calcValuePosInBarGraphWidget(line2Data, min, max, d);
    bool drawLine2 = pLine2 >= 0 && pLine2 < d;

    if (drawLine1 && drawLine2) {
        // make sure line positions don't overlap
        if (pLine1 == pLine2) {
            pLine1 = pLine2 - 1;
        }

        // make sure all lines are visible
        if (pLine1 < 0) {
            pLine2 -= pLine1;
            pLine1 = 0;
        }
    }

    BlinkOpacity blinkOpacityScope(blinkOpacity);

    // pressed: the color and the background swapped
    uint16_t fg = color;
    uint16_t bg = backgroundColor;
    swapColorsIfActive(flags.active, fg, bg);

    Style textStyle;
    memcpy(&textStyle, widget->textStyle ? getStyle(widget->textStyle) : style, sizeof(Style));
    if (style->color != color) {
        textStyle.color = fg;
    }

    if (horizontal) {
        // calc text position
        char valueText[64];
        int pText = 0;
        int wText = 0;
        if (!(widget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
            if (widget->textStyle) {
                font::Font font = styleGetFont(&textStyle);

                textData.toText(valueText, sizeof(valueText));
                wText = display::measureStr(valueText, -1, font, w);

                int padding = textStyle.paddingLeft;
                wText += padding;

                if (pValue + wText <= d) {
                    textStyle.backgroundColor = bg;
                    pText = pValue;
                } else {
                    textStyle.color = textStyle.backgroundColor;
                    textStyle.backgroundColor = fg;
                    wText += padding;
                    pText = MAX(pValue - wText, 0);
                }
            }
        } else {
            pText = pValue;
        }

        if ((widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x, y, x + pText - 1, y + h - 1);
            }

            if (!(widget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
                drawText(valueText, -1, x + pText, y, wText, h, &textStyle);
            }

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + wText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x + pBackground, y, x + pLine1 - 1, y + h - 1);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x + pBackground, y, x + pLine2 - 1, y + h - 1);
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x + pBackground, y, x + d - 1, y + h - 1);
            }
        } else {
            x += w - 1;

            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x - (pText - 1), y, x, y + h - 1);
            }

            if (!(widget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
                drawText(valueText, -1, x - (pText + wText - 1), y, wText, h, &textStyle);
            }

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + wText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x - (pLine1 - 1), y, x - pBackground, y + h - 1);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x - (pLine2 - 1), y, x - pBackground, y + h - 1);
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x - (d - 1), y, x - pBackground, y + h - 1);
            }
        }

        if (drawLine1) {
            drawLineInBarGraphWidget(widget, pLine1, widget->line1Style, x, y, w, h);
        }
        if (drawLine2) {
            drawLineInBarGraphWidget(widget, pLine2, widget->line2Style, x, y, w, h);
        }
    } else {
        // calc text position
        char valueText[64];
        int pText = 0;
        int hText = 0;
        if (widget->textStyle) {
            font::Font font = styleGetFont(&textStyle);

            textData.toText(valueText, sizeof(valueText));
            hText = font.getHeight();

            int padding = textStyle.paddingTop;
            hText += padding;

            if (pValue + hText <= d) {
                textStyle.backgroundColor = bg;
                pText = pValue;
            } else {
                textStyle.color = textStyle.backgroundColor;
                textStyle.backgroundColor = fg;
                hText += padding;
                pText = MAX(pValue - hText, 0);
            }
        }

        if ((widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x, y, x + w - 1, y + pText - 1);
            }

            drawText(valueText, -1, x, y + pText, w, hText, &textStyle);

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + hText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x, y + pBackground, x + w - 1, y + pLine1 - 1);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x, y + pBackground, x + w - 1, y + pLine2 - 1);
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x, y + pBackground, x + w - 1, y + d - 1);
            }
        } else {
            y += h - 1;

            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x, y - (pText - 1), x + w - 1, y);
            }

            drawText(valueText, -1, x, y - (pText + hText - 1), w, hText, &textStyle);

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + hText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x, y - (pLine1 - 1), x + w - 1, y - pBackground);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x, y - (pLine2 - 1), x + w - 1, y - (pBackground));
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x, y - (d - 1), x + w - 1, y - pBackground);
            }
        }

        if (drawLine1) {
            drawLineInBarGraphWidget(widget, pLine1, widget->line1Style, x, y, w, h);
        }
        if (drawLine2) {
            drawLineInBarGraphWidget(widget, pLine2, widget->line2Style, x, y, w, h);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// The bar as a track (see BAR_GRAPH_FLAG_...)

namespace {

static const int TICK_LENGTH = 4;
static const int SCALE_LABEL_GAP = 3; // between a scale number and its tick
static const int TRACK_GAP = 3; // between the ticks and the track
static const int MARKER_HEIGHT = 7; // from the tip of the triangle to its base
static const int LINE1_OVERHANG = 2; // the marked line 1 is longer than the track is wide, on each side
static const int MARKER_GAP = LINE1_OVERHANG + 2; // between the track and the tip of the triangle
static const int MARKER_HALF_WIDTH = 5;
// the label beside the triangle: right of it, MARKER_LABEL_LIFT px higher
// than its middle above a horizontal track; MARKER_LABEL_GAP px right of a
// vertical track's triangle
static const int MARKER_LABEL_GAP = 3;
static const int MARKER_LABEL_LIFT = 3;
static const int MARKER_LABEL_GAP_VERTICAL = 6;
static const int MAX_SCALE_INTERVALS = 6;
static const int MAX_SCALE_TICKS = 32;
static const uint8_t ZONE_OPACITY = 72;

struct Track {
    // the track, pixels inclusive
    int x1, y1, x2, y2;
    int orientation; // BAR_GRAPH_ORIENTATION_...
    int d; // the length in pixels
    int radii[8];
    bool rounded;

    bool isHorizontal() const {
        return orientation == BAR_GRAPH_ORIENTATION_LEFT_RIGHT || orientation == BAR_GRAPH_ORIENTATION_RIGHT_LEFT;
    }

    // the pixels p1 .. p2 - 1 from the start of the bar, the whole track across
    void part(int p1, int p2, int &rx1, int &ry1, int &rx2, int &ry2) const {
        rx1 = x1;
        ry1 = y1;
        rx2 = x2;
        ry2 = y2;
        if (orientation == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
            rx1 = x1 + p1;
            rx2 = x1 + p2 - 1;
        } else if (orientation == BAR_GRAPH_ORIENTATION_RIGHT_LEFT) {
            rx1 = x2 - (p2 - 1);
            rx2 = x2 - p1;
        } else if (orientation == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
            ry1 = y1 + p1;
            ry2 = y1 + p2 - 1;
        } else {
            ry1 = y2 - (p2 - 1);
            ry2 = y2 - p1;
        }
    }

    // the x (horizontal) or y (vertical) of the pixel edge at p from the start
    float edge(int p) const {
        if (orientation == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
            return (float)(x1 + p);
        } else if (orientation == BAR_GRAPH_ORIENTATION_RIGHT_LEFT) {
            return (float)(x2 + 1 - p);
        } else if (orientation == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
            return (float)(y1 + p);
        } else {
            return (float)(y2 + 1 - p);
        }
    }

    // fills the pixels p1 .. p2 - 1 from the start inside the rounded corners
    void fill(int p1, int p2, uint16_t colorIndex, uint8_t opacity = 255) const {
        if (p1 < 0) {
            p1 = 0;
        }
        if (p2 > d) {
            p2 = d;
        }
        if (p2 <= p1) {
            return;
        }

        int cx1, cy1, cx2, cy2;
        part(p1, p2, cx1, cy1, cx2, cy2);

        uint8_t savedOpacity = display::getOpacity();
        if (opacity != 255) {
            display::setOpacity((uint8_t)(savedOpacity * opacity / 255));
        }

        if (rounded) {
            display::setBackColor(colorIndex);
            display::fillRoundedRect(
                x1, y1, x2, y2, 0,
                radii[0], radii[1], radii[2], radii[3],
                radii[4], radii[5], radii[6], radii[7],
                false, true,
                cx1, cy1, cx2, cy2
            );
        } else {
            display::setColor(colorIndex);
            display::fillRect(cx1, cy1, cx2, cy2);
        }

        display::setOpacity(savedOpacity);
    }
};

struct Scale {
    float ticks[MAX_SCALE_TICKS];
    int numTicks;
    int decimals;
    float factor; // a number of the scale is the value / factor
    const char *unit; // after the last number, nullptr: none

    void label(int i, char *text, size_t size) const {
        float value = ticks[i] / factor;
        if (fabsf(value) < 1E-6f) {
            value = 0; // no "-0"
        }
        if (unit && i == numTicks - 1) {
            snprintf(text, size, "%.*f %s", decimals, value, unit);
        } else {
            snprintf(text, size, "%.*f", decimals, value);
        }
    }
};

// 1, 2, 2.5 or 5 times a power of 10, so that there are at most maxIntervals
static float scaleStep(float range, int maxIntervals) {
    float raw = range / maxIntervals;
    float magnitude = powf(10.0f, floorf(log10f(raw)));
    float n = raw / magnitude;
    float step;
    if (n <= 1.0f + 1E-4f) {
        step = 1.0f;
    } else if (n <= 2.0f + 1E-4f) {
        step = 2.0f;
    } else if (n <= 2.5f + 1E-4f) {
        step = 2.5f;
    } else if (n <= 5.0f + 1E-4f) {
        step = 5.0f;
    } else {
        step = 10.0f;
    }
    return step * magnitude;
}

static void makeScale(Scale &scale, float min, float max, int maxIntervals, Unit unit) {
    scale.numTicks = 0;
    scale.decimals = 0;
    scale.factor = 1.0f;
    scale.unit = nullptr;

    if (unit != UNIT_UNKNOWN) {
        Unit baseUnit = getBaseUnit(unit);
        Unit displayUnit = getBiggestUnit(baseUnit, MAX(fabsf(min), fabsf(max)));
        if (displayUnit == UNIT_UNKNOWN) {
            displayUnit = baseUnit;
        }
        scale.factor = getUnitFactor(displayUnit);
        scale.unit = getUnitName(displayUnit);
        if (!*scale.unit) {
            scale.unit = nullptr;
        }
    }

    if (maxIntervals < 1) {
        maxIntervals = 1;
    }
    if (maxIntervals > MAX_SCALE_INTERVALS) {
        maxIntervals = MAX_SCALE_INTERVALS;
    }

    float step = scaleStep(max - min, maxIntervals);

    // the decimals of the step in the scale unit
    float stepInUnit = step / scale.factor;
    while (scale.decimals < 6) {
        float m = stepInUnit * powf(10.0f, (float)scale.decimals);
        if (fabsf(m - roundf(m)) < 1E-3f * m) {
            break;
        }
        scale.decimals++;
    }

    float epsilon = step * 1E-3f;
    for (float t = ceilf((min - epsilon) / step) * step; t <= max + epsilon && scale.numTicks < MAX_SCALE_TICKS; t += step) {
        scale.ticks[scale.numTicks++] = t;
    }
}

// The label with the top of its ink (inkAbove px above the baseline) at inkTop.
// Labels are placed by their ink: the line height of a font has room above and
// below the glyphs of numbers and short labels.
static void drawLabel(const char *text, int x, int inkTop, int inkAbove, font::Font &font, uint16_t color) {
    int y = inkTop + inkAbove - font.getAscent();
    display::setColor(color);
    display::drawStr(text, -1, x, y, x, y, x + display::measureStr(text, -1, font) - 1, y + font.getHeight() - 1, font, -1);
}

// keeps p .. p + size - 1 inside from .. to
static int clampTo(int p, int size, int from, int to) {
    if (p + size - 1 > to) {
        p = to - size + 1;
    }
    if (p < from) {
        p = from;
    }
    return p;
}

} // namespace

void BarGraphWidgetState::renderTrack(const BarGraphWidget *widget, const Style *style, float min, float max) {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    const int x = widgetCursor.x;
    const int y = widgetCursor.y;
    const int w = widgetCursor.w;
    const int h = widgetCursor.h;

    BlinkOpacity blinkOpacityScope(blinkOpacity);

    // pressed: the color and the background swapped
    uint16_t fg = color;
    uint16_t bg = backgroundColor;
    swapColorsIfActive(flags.active, fg, bg);

    Track t;
    t.orientation = widget->orientation & BAR_GRAPH_ORIENTATION_MASK;
    bool horizontal = t.isHorizontal();

    // the content box, inside the padding
    int cx1 = x + style->paddingLeft;
    int cy1 = y + style->paddingTop;
    int cx2 = x + w - 1 - style->paddingRight;
    int cy2 = y + h - 1 - style->paddingBottom;
    int cw = cx2 - cx1 + 1;
    int ch = cy2 - cy1 + 1;
    if (cw <= 0 || ch <= 0) {
        return;
    }

    bool validRange = max > min;

    // scale
    const Style *scaleStyle = widget->scaleStyle ? getStyle(widget->scaleStyle) : nullptr;
    font::Font scaleFont;
    Scale scale;
    scale.numTicks = 0;
    int scaleLabelWidth = 0;
    int scaleInk = 0;
    if (scaleStyle && validRange) {
        scaleFont = styleGetFont(scaleStyle);
        if (scaleFont) {
            scaleInk = display::measureStrInkAbove("0123456789", -1, scaleFont);
            int maxIntervals;
            if (horizontal) {
                char text[32];
                snprintf(text, sizeof(text), "%g", fabsf(max) > fabsf(min) ? max : min);
                maxIntervals = cw / (display::measureStr(text, -1, scaleFont) + 3 * scaleInk);
            } else {
                maxIntervals = ch / (2 * scaleInk);
            }
            makeScale(scale, min, max, maxIntervals, horizontal ? data.getUnit() : UNIT_UNKNOWN);

            for (int i = 0; i < scale.numTicks; i++) {
                char text[32];
                scale.label(i, text, sizeof(text));
                int labelWidth = display::measureStr(text, -1, scaleFont);
                if (labelWidth > scaleLabelWidth) {
                    scaleLabelWidth = labelWidth;
                }
            }
        }
    }
    bool hasScale = scale.numTicks > 0;

    // marker of line 1 and its label
    bool hasMarker = (widget->flags & BAR_GRAPH_FLAG_LINE1_MARKER) != 0;
    const Style *line1Style = getStyle(widget->line1Style);
    char markerLabel[32];
    markerLabel[0] = 0;
    font::Font markerFont;
    int markerLabelWidth = 0;
    int markerInk = 0;
    if (hasMarker && !line1Label.isUndefinedOrNull()) {
        markerFont = styleGetFont(line1Style);
        if (markerFont) {
            line1Label.toText(markerLabel, sizeof(markerLabel));
            if (markerLabel[0]) {
                markerLabelWidth = display::measureStr(markerLabel, -1, markerFont);
                markerInk = display::measureStrInkAbove(markerLabel, -1, markerFont);
            }
        }
    }

    // place the track
    if (horizontal) {
        // below the track: the scale, else the end of the marked line 1
        int scaleHeight = hasScale ? TRACK_GAP + TICK_LENGTH + SCALE_LABEL_GAP + scaleInk : hasMarker ? LINE1_OVERHANG : 0;
        int markerHeight = 0;
        if (hasMarker) {
            markerHeight = MARKER_GAP + MARKER_HEIGHT;
            if (markerLabelWidth > 0) {
                // the top of the label ink (see the label below)
                int labelTop = MARKER_GAP + (MARKER_HEIGHT + markerInk + 1) / 2 + MARKER_LABEL_LIFT;
                if (labelTop > markerHeight) {
                    markerHeight = labelTop;
                }
            }
            markerHeight += 1;
        }
        int available = ch - scaleHeight - markerHeight;
        int trackHeight = widget->thickness > 0 && widget->thickness < available ? widget->thickness : available;
        if (trackHeight <= 0) {
            return;
        }
        int top = cy1 + (ch - (markerHeight + trackHeight + scaleHeight)) / 2;
        t.x1 = cx1;
        t.x2 = cx2;
        t.y1 = top + markerHeight;
        t.y2 = t.y1 + trackHeight - 1;
        t.d = cw;
    } else {
        // left of the track: the scale, else the end of the marked line 1
        int scaleWidth = hasScale ? scaleLabelWidth + SCALE_LABEL_GAP + TICK_LENGTH + TRACK_GAP : hasMarker ? LINE1_OVERHANG : 0;
        int markerWidth = 0;
        if (hasMarker) {
            markerWidth = MARKER_GAP + MARKER_HEIGHT + 1;
            if (markerLabelWidth > 0) {
                markerWidth += MARKER_LABEL_GAP_VERTICAL + markerLabelWidth;
            }
        }
        int available = cw - scaleWidth - markerWidth;
        int trackWidth = widget->thickness > 0 && widget->thickness < available ? widget->thickness : available;
        if (trackWidth <= 0) {
            return;
        }
        int left = cx1 + (cw - (scaleWidth + trackWidth + markerWidth)) / 2;
        t.x1 = left + scaleWidth;
        t.x2 = t.x1 + trackWidth - 1;
        t.y1 = cy1;
        t.y2 = cy2;
        t.d = ch;
    }

    t.radii[0] = style->borderRadiusTLX;
    t.radii[1] = style->borderRadiusTLY;
    t.radii[2] = style->borderRadiusTRX;
    t.radii[3] = style->borderRadiusTRY;
    t.radii[4] = style->borderRadiusBRX;
    t.radii[5] = style->borderRadiusBRY;
    t.radii[6] = style->borderRadiusBLX;
    t.radii[7] = style->borderRadiusBLY;
    t.rounded = false;
    for (int i = 0; i < 8; i++) {
        if (t.radii[i]) {
            t.rounded = true;
        }
    }

    // track
    t.fill(0, t.d, bg);

    if (!validRange) {
        return;
    }

    // -1 below min, d + 1 above max (also a value too large for an int), -1 for NaN
    auto pos = [&](const Value &value) {
        float p = roundf((value.toFloat() - min) * t.d / (max - min));
        if (!(p >= -1.0f)) {
            return -1;
        }
        if (p > t.d + 1.0f) {
            return t.d + 1;
        }
        return (int)p;
    };
    auto clampPos = [&](int p) {
        return p < 0 ? 0 : p > t.d ? t.d : p;
    };

    int pValue = clampPos(pos(data));

    bool hasLine1 = widget->line1Data && !line1Data.isUndefinedOrNull();
    int pLine1 = hasLine1 ? pos(line1Data) : 0;
    bool drawLine1 = hasLine1 && pLine1 >= 0 && pLine1 <= t.d;

    // the dimmed part up to line 1
    if ((widget->flags & BAR_GRAPH_FLAG_ZONE) && hasLine1) {
        int pZone = clampPos(pLine1);
        if (pZone > pValue) {
            t.fill(pValue, pZone, fg, ZONE_OPACITY);
        }
    }

    // bar
    t.fill(0, pValue, fg);

    // line 2
    if (widget->line2Data && !line2Data.isUndefinedOrNull()) {
        int pLine2 = pos(line2Data);
        if (pLine2 >= 0 && pLine2 <= t.d && !(drawLine1 && pLine2 == pLine1)) {
            int p = pLine2 < t.d ? pLine2 : t.d - 1;
            t.fill(p, p + 1, getStyle(widget->line2Style)->color);
        }
    }

    // line 1 and its marker
    if (drawLine1) {
        if (hasMarker) {
            int p1 = pLine1 - 1;
            if (p1 > t.d - 2) {
                p1 = t.d - 2;
            }
            if (p1 < 0) {
                p1 = 0;
            }
            // two pixels wide, LINE1_OVERHANG longer than the track is wide
            // on each side
            int lx1, ly1, lx2, ly2;
            t.part(p1, p1 + 2, lx1, ly1, lx2, ly2);
            if (horizontal) {
                ly1 -= LINE1_OVERHANG;
                ly2 += LINE1_OVERHANG;
            } else {
                lx1 -= LINE1_OVERHANG;
                lx2 += LINE1_OVERHANG;
            }
            display::setColor(line1Style->color);
            display::fillRect(lx1, ly1, lx2, ly2);

            float c = t.edge(pLine1);
            display::Path path;
            if (horizontal) {
                float tipY = (float)(t.y1 - MARKER_GAP);
                path.moveTo(c, tipY);
                path.lineTo(c - MARKER_HALF_WIDTH, tipY - MARKER_HEIGHT);
                path.lineTo(c + MARKER_HALF_WIDTH, tipY - MARKER_HEIGHT);
            } else {
                float tipX = (float)(t.x2 + 1 + MARKER_GAP);
                path.moveTo(tipX, c);
                path.lineTo(tipX + MARKER_HEIGHT, c - MARKER_HALF_WIDTH);
                path.lineTo(tipX + MARKER_HEIGHT, c + MARKER_HALF_WIDTH);
            }
            path.close();
            display::fillPath(path);

            if (markerLabelWidth > 0) {
                // the ink of the label centered on the triangle
                int lx, inkTop;
                if (horizontal) {
                    int ci = (int)c;
                    lx = ci + MARKER_HALF_WIDTH + MARKER_LABEL_GAP;
                    if (lx + markerLabelWidth - 1 > x + w - 1) {
                        // no room right of the triangle
                        lx = ci - MARKER_HALF_WIDTH - MARKER_LABEL_GAP - markerLabelWidth;
                    }
                    lx = clampTo(lx, markerLabelWidth, x, x + w - 1);
                    inkTop = t.y1 - MARKER_GAP - (MARKER_HEIGHT + markerInk + 1) / 2 - MARKER_LABEL_LIFT;
                } else {
                    lx = t.x2 + 1 + MARKER_GAP + MARKER_HEIGHT + MARKER_LABEL_GAP_VERTICAL;
                    inkTop = (int)c - markerInk / 2;
                }
                inkTop = clampTo(inkTop, markerInk, y, y + h - 1);
                drawLabel(markerLabel, lx, inkTop, markerInk, markerFont, line1Style->color);
            }
        } else {
            int p = pLine1 < t.d ? pLine1 : t.d - 1;
            t.fill(p, p + 1, line1Style->color);
        }
    }

    // scale
    if (hasScale) {
        for (int i = 0; i < scale.numTicks; i++) {
            // the tick on the pixel of the bar at the tick value
            int p = clampPos(pos(FloatValue(scale.ticks[i])));
            if (p > t.d - 1) {
                p = t.d - 1;
            }
            int px1, py1, px2, py2;
            t.part(p, p + 1, px1, py1, px2, py2);

            char text[32];
            scale.label(i, text, sizeof(text));
            int labelWidth = display::measureStr(text, -1, scaleFont);

            display::setColor(scaleStyle->color);
            if (horizontal) {
                int tx = px1;
                display::drawVLine(tx, t.y2 + 1 + TRACK_GAP, TICK_LENGTH - 1);
                int lx = clampTo(tx - labelWidth / 2, labelWidth, x, x + w - 1);
                drawLabel(text, lx, t.y2 + 1 + TRACK_GAP + TICK_LENGTH + SCALE_LABEL_GAP, scaleInk, scaleFont, scaleStyle->color);
            } else {
                int ty = py1;
                int tickX2 = t.x1 - TRACK_GAP - 1;
                display::drawHLine(tickX2 - (TICK_LENGTH - 1), ty, TICK_LENGTH - 1);
                int lx = tickX2 - (TICK_LENGTH - 1) - SCALE_LABEL_GAP - labelWidth;
                // the ink centered on the tick
                int inkTop = clampTo(ty - scaleInk / 2, scaleInk, y, y + h - 1);
                drawLabel(text, lx, inkTop, scaleInk, scaleFont, scaleStyle->color);
            }
        }
    }

    // the value in the track, after the end of the bar if it fits there, else
    // at the end inside the bar
    if (!(widget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE) && widget->textStyle) {
        const Style *textStyle = getStyle(widget->textStyle);
        font::Font font = styleGetFont(textStyle);
        if (font) {
            char valueText[64];
            textData.toText(valueText, sizeof(valueText));
            int textWidth = display::measureStr(valueText, -1, font);
            int textHeight = font.getHeight();
            int trackWidth = t.x2 - t.x1 + 1;
            int trackHeight = t.y2 - t.y1 + 1;

            int along = horizontal ? textWidth + textStyle->paddingLeft : textHeight + textStyle->paddingTop;
            bool outside = pValue + along <= t.d;
            uint16_t textColor = outside ? (style->color != color ? fg : textStyle->color) : bg;

            int tx, ty;
            if (horizontal) {
                ty = t.y1 + (trackHeight - textHeight) / 2;
                int p = outside ? pValue + textStyle->paddingLeft : pValue - textStyle->paddingLeft - textWidth;
                if (p < 0) {
                    p = 0;
                }
                tx = t.orientation == BAR_GRAPH_ORIENTATION_LEFT_RIGHT ? t.x1 + p : t.x2 + 1 - p - textWidth;
            } else {
                tx = t.x1 + (trackWidth - textWidth) / 2;
                int p = outside ? pValue + textStyle->paddingTop : pValue - textStyle->paddingTop - textHeight;
                if (p < 0) {
                    p = 0;
                }
                ty = t.orientation == BAR_GRAPH_ORIENTATION_TOP_BOTTOM ? t.y1 + p : t.y2 + 1 - p - textHeight;
            }

            display::setColor(textColor);
            display::drawStr(valueText, -1, tx, ty, t.x1, t.y1, t.x2, t.y2, font, -1);
        }
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
