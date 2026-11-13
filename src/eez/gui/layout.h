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

// Rows and columns: a ContainerWidget with the layout horizontal (a row) or
// vertical (a column) places its children one after the other on the main axis
// (x in a row, y in a column), a subset of the CSS flexbox without wrap:
//
// - The content box is the container box without the borders and the padding
//   of its style. The children are separated by the gap.
// - The size of a child on each axis is its own size, or the size of its
//   content with WIDGET_FLAG_FIT_WIDTH / WIDGET_FLAG_FIT_HEIGHT (Text and
//   DisplayData: the text in the font of the style with the padding and the
//   borders; a row or column: its visible children). A hidden child takes no
//   space.
// - The free space of the main axis is shared by the children with
//   WIDGET_FLAG_GROW (the first ones get the remaining pixels). Without growing
//   children it is placed by justifyContent: start, center, end, or between
//   the children (space between). The children are not shrunk when they
//   don't fit.
// - On the cross axis each child is aligned by its align self (or alignItems of
//   the container): start, center, end, stretch (the size of the content box)
//   or baseline (in a row the first text lines of the children share one
//   baseline, the lowest one; a child without text has its baseline at its
//   bottom; in a column it is the start).
// - With a right-to-left language (g_isRTL) the positions are mirrored.
//
// The layout is done every time the children are enumerated, so hidden
// children, changed texts and a changed container size are laid out again.
// When the result changes, the container and its children are drawn again.

#pragma once

#include <stdint.h>

namespace eez {
namespace gui {

struct Widget;
struct WidgetCursor;
struct ContainerWidget;

#define LAYOUT_JUSTIFY_START 0
#define LAYOUT_JUSTIFY_CENTER 1
#define LAYOUT_JUSTIFY_END 2
#define LAYOUT_JUSTIFY_SPACE_BETWEEN 3

#define LAYOUT_ALIGN_AUTO 0
#define LAYOUT_ALIGN_START 1
#define LAYOUT_ALIGN_CENTER 2
#define LAYOUT_ALIGN_END 3
#define LAYOUT_ALIGN_STRETCH 4
#define LAYOUT_ALIGN_BASELINE 5

// the children after this many are not shown
#define LAYOUT_MAX_CHILDREN 64

// a child of a row or column: its box relative to the container box, a hidden
// child has the size 0
struct LayoutChild {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    bool visible;
};

bool isRowOrColumn(const Widget *widget);

// The children of the row or column widgetCursor.widget in its box of the size
// widgetCursor.w x widgetCursor.h. children has room for LAYOUT_MAX_CHILDREN.
// Returns the number of the laid out children and a hash of the result.
int layoutChildren(const WidgetCursor &widgetCursor, LayoutChild *children, uint32_t &hash);

// The child of a row or column that is enumerated: its size is set by the
// layout, the child doesn't change it (a Text widget with fit to content).
extern const Widget *g_layoutPlacedWidget;

} // namespace gui
} // namespace eez
