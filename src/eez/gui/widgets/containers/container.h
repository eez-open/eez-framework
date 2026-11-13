/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#pragma once

namespace eez {
namespace gui {

// static: the children at their own position (pinned and resized with the
// container); horizontal (a row) and vertical (a column): see gui/layout.h
#define CONTAINER_WIDGET_LAYOUT_STATIC 0
#define CONTAINER_WIDGET_LAYOUT_HORIZONTAL 1
#define CONTAINER_WIDGET_LAYOUT_VERTICAL 2

struct ContainerWidget : public Widget {
	ListOfAssetsPtr<Widget> widgets;
	uint16_t flags;
	int16_t overlay;
    uint8_t layout; // CONTAINER_WIDGET_LAYOUT_...
    // a row or column: the main axis (LAYOUT_JUSTIFY_...) and the cross axis
    // (LAYOUT_ALIGN_..., LAYOUT_ALIGN_AUTO is the start) alignment of the
    // children and the space between them
    uint8_t justifyContent;
    uint8_t alignItems;
    uint8_t reserved1;
    int16_t gap;
    int16_t reserved2;
};

struct ContainerWidgetState : public WidgetState {
	WidgetStateFlags flags;
	int styleId;
	Overlay *overlay;
    int overlayState;
    int displayBufferIndex;
	bool displayBufferSelected;
	bool repainted;
    // a row or column: the hash of the last layout, the container is drawn
    // again with its children when the layout changes
    uint32_t layoutHash = 0;

    bool updateState() override;
	void render() override;
	void enumChildren() override;
	void renderOverlayChildren();
};

} // namespace gui
} // namespace eez
