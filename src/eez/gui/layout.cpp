/*
 * EEZ Modular Firmware
 * Copyright (C) 2025-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/gui/gui.h>
#include <eez/gui/layout.h>
#include <eez/gui/widgets/containers/container.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/display_data.h>

namespace eez {
namespace gui {

const Widget *g_layoutPlacedWidget;

bool isRowOrColumn(const Widget *widget) {
    if (widget->type != WIDGET_TYPE_CONTAINER) {
        return false;
    }
    auto layout = ((const ContainerWidget *)widget)->layout;
    return layout == CONTAINER_WIDGET_LAYOUT_HORIZONTAL || layout == CONTAINER_WIDGET_LAYOUT_VERTICAL;
}

// the style of the widget cursor.widget after the overrideStyle hook (not of a
// state: the layout doesn't change when a widget is pressed)
static const Style *getLayoutStyle(const WidgetCursor &cursor) {
    return getStyle(overrideStyle(cursor, cursor.widget->style));
}

static bool isChildVisible(WidgetCursor &cursor, const Widget *child) {
    if (!child->visible) {
        return true;
    }
    cursor.widget = child;
    return get(cursor, child->visible).toBool();
}

static void getRowOrColumnContentSize(WidgetCursor &cursor, int &w, int &h);

// the size of a child of a row or column before it grows or stretches: its own
// size or the size of its content
static void getChildSize(WidgetCursor &cursor, const Widget *child, int &w, int &h) {
    w = child->width;
    h = child->height;

    if (!(child->flags & (WIDGET_FLAG_FIT_WIDTH | WIDGET_FLAG_FIT_HEIGHT))) {
        return;
    }

    int contentW = w;
    int contentH = h;

    cursor.widget = child;
    if (child->type == WIDGET_TYPE_TEXT) {
        TextWidget_getContentSize(cursor, contentW, contentH);
    } else if (child->type == WIDGET_TYPE_DISPLAY_DATA) {
        DisplayDataWidget_getContentSize(cursor, contentW, contentH);
    } else if (isRowOrColumn(child)) {
        getRowOrColumnContentSize(cursor, contentW, contentH);
    }

    if (child->flags & WIDGET_FLAG_FIT_WIDTH) {
        w = contentW;
    }
    if (child->flags & WIDGET_FLAG_FIT_HEIGHT) {
        h = contentH;
    }
}

// the size of the visible children of the row or column cursor.widget with
// the gaps, the padding and the borders
static void getRowOrColumnContentSize(WidgetCursor &cursor, int &w, int &h) {
    auto container = (const ContainerWidget *)cursor.widget;
    const Style *style = getLayoutStyle(cursor);
    bool row = container->layout == CONTAINER_WIDGET_LAYOUT_HORIZONTAL;

    int main = 0;
    int cross = 0;
    int numVisible = 0;

    auto &widgets = container->widgets;
    for (uint32_t index = 0; index < widgets.count && index < LAYOUT_MAX_CHILDREN; index++) {
        auto child = widgets[index];
        WidgetCursor childCursor = cursor;
        if (!isChildVisible(childCursor, child)) {
            continue;
        }

        int childW, childH;
        getChildSize(childCursor, child, childW, childH);

        main += row ? childW : childH;
        int childCross = row ? childH : childW;
        if (childCross > cross) {
            cross = childCross;
        }
        numVisible++;
    }

    if (numVisible > 1) {
        main += container->gap * (numVisible - 1);
    }

    int insetW = style->borderSizeLeft + style->paddingLeft + style->paddingRight + style->borderSizeRight;
    int insetH = style->borderSizeTop + style->paddingTop + style->paddingBottom + style->borderSizeBottom;

    w = (row ? main : cross) + insetW;
    h = (row ? cross : main) + insetH;
}

// the y of the baseline of the first text line in the child box of the height
// h, the bottom of the box for a child without text
static int getChildBaseline(WidgetCursor &cursor, const Widget *child, int h) {
    if (child->type == WIDGET_TYPE_TEXT || child->type == WIDGET_TYPE_DISPLAY_DATA) {
        cursor.widget = child;
        int baseline = getTextBaseline(getLayoutStyle(cursor), h);
        if (baseline >= 0) {
            return baseline;
        }
    }
    return h;
}

static void hashValue(uint32_t &hash, int value) {
    // FNV-1a
    for (int i = 0; i < 2; i++) {
        hash ^= (uint8_t)(value >> (8 * i));
        hash *= 16777619u;
    }
}

int layoutChildren(const WidgetCursor &widgetCursor, LayoutChild *children, uint32_t &hash) {
    auto container = (const ContainerWidget *)widgetCursor.widget;
    bool row = container->layout == CONTAINER_WIDGET_LAYOUT_HORIZONTAL;

    WidgetCursor cursor = widgetCursor;
    const Style *style = getLayoutStyle(cursor);

    int left = style->borderSizeLeft + style->paddingLeft;
    int top = style->borderSizeTop + style->paddingTop;
    int contentW = widgetCursor.w - left - style->paddingRight - style->borderSizeRight;
    int contentH = widgetCursor.h - top - style->paddingBottom - style->borderSizeBottom;
    int mainSize = row ? contentW : contentH;
    int crossSize = row ? contentH : contentW;

    auto &widgets = container->widgets;
    int count = widgets.count < LAYOUT_MAX_CHILDREN ? (int)widgets.count : LAYOUT_MAX_CHILDREN;

    uint8_t aligns[LAYOUT_MAX_CHILDREN];
    int16_t baselines[LAYOUT_MAX_CHILDREN];

    // the sizes
    int numVisible = 0;
    int numGrow = 0;
    int total = 0;
    for (int i = 0; i < count; i++) {
        auto child = widgets[i];
        auto &layoutChild = children[i];

        layoutChild.visible = isChildVisible(cursor, child);
        if (!layoutChild.visible) {
            layoutChild.w = 0;
            layoutChild.h = 0;
            continue;
        }

        int w, h;
        getChildSize(cursor, child, w, h);

        int align = (child->flags & WIDGET_FLAG_ALIGN_SELF_MASK) >> WIDGET_FLAG_ALIGN_SELF_SHIFT;
        if (align == LAYOUT_ALIGN_AUTO) {
            align = container->alignItems;
        }
        if (align == LAYOUT_ALIGN_AUTO || (align == LAYOUT_ALIGN_BASELINE && !row)) {
            align = LAYOUT_ALIGN_START;
        }
        aligns[i] = (uint8_t)align;

        if (align == LAYOUT_ALIGN_STRETCH) {
            if (row) {
                h = contentH;
            } else {
                w = contentW;
            }
        }

        layoutChild.w = (int16_t)w;
        layoutChild.h = (int16_t)h;

        total += row ? w : h;
        numVisible++;
        if (child->flags & WIDGET_FLAG_GROW) {
            numGrow++;
        }
    }

    if (numVisible > 1) {
        total += container->gap * (numVisible - 1);
    }

    int freeSpace = mainSize - total;

    // the growing children share the free space
    if (freeSpace > 0 && numGrow > 0) {
        int share = freeSpace / numGrow;
        int rest = freeSpace % numGrow;
        for (int i = 0; i < count; i++) {
            if (children[i].visible && (widgets[i]->flags & WIDGET_FLAG_GROW)) {
                int add = share + (rest > 0 ? 1 : 0);
                if (rest > 0) {
                    rest--;
                }
                if (row) {
                    children[i].w += add;
                } else {
                    children[i].h += add;
                }
            }
        }
        freeSpace = 0;
    }

    // the free space on the main axis
    int pos = 0;
    int extraGap = 0;
    int extraGapRest = 0;
    if (freeSpace > 0) {
        if (container->justifyContent == LAYOUT_JUSTIFY_CENTER) {
            pos = freeSpace / 2;
        } else if (container->justifyContent == LAYOUT_JUSTIFY_END) {
            pos = freeSpace;
        } else if (container->justifyContent == LAYOUT_JUSTIFY_SPACE_BETWEEN && numVisible > 1) {
            extraGap = freeSpace / (numVisible - 1);
            extraGapRest = freeSpace % (numVisible - 1);
        }
    }

    // the baseline of the row: the lowest baseline of the children aligned to it
    int rowBaseline = -1;
    for (int i = 0; i < count; i++) {
        if (children[i].visible && aligns[i] == LAYOUT_ALIGN_BASELINE) {
            baselines[i] = (int16_t)getChildBaseline(cursor, widgets[i], children[i].h);
            if (baselines[i] > rowBaseline) {
                rowBaseline = baselines[i];
            }
        }
    }

    // the positions
    int numPlaced = 0;
    for (int i = 0; i < count; i++) {
        auto &layoutChild = children[i];

        int mainPos = pos;
        int crossPos = 0;

        if (layoutChild.visible) {
            int childCross = row ? layoutChild.h : layoutChild.w;
            if (aligns[i] == LAYOUT_ALIGN_CENTER) {
                crossPos = (crossSize - childCross) / 2;
            } else if (aligns[i] == LAYOUT_ALIGN_END) {
                crossPos = crossSize - childCross;
            } else if (aligns[i] == LAYOUT_ALIGN_BASELINE) {
                crossPos = rowBaseline - baselines[i];
            }

            pos += (row ? layoutChild.w : layoutChild.h) + container->gap + extraGap;
            if (numPlaced < extraGapRest) {
                pos++;
            }
            numPlaced++;
        }

        if (row) {
            layoutChild.x = (int16_t)(left + mainPos);
            layoutChild.y = (int16_t)(top + crossPos);
        } else {
            layoutChild.x = (int16_t)(left + crossPos);
            layoutChild.y = (int16_t)(top + mainPos);
        }

        if (g_isRTL) {
            layoutChild.x = (int16_t)(widgetCursor.w - (layoutChild.x + layoutChild.w));
        }
    }

    hash = 2166136261u;
    hashValue(hash, widgetCursor.w);
    hashValue(hash, widgetCursor.h);
    for (int i = 0; i < count; i++) {
        hashValue(hash, children[i].visible);
        hashValue(hash, children[i].x);
        hashValue(hash, children[i].y);
        hashValue(hash, children[i].w);
        hashValue(hash, children[i].h);
    }

    return count;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
