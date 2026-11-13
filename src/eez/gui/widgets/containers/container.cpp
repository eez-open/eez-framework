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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/core/debug.h>
#include <eez/core/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/container.h>

#include <eez/flow/private.h>

namespace eez {
namespace gui {

// the scrolling of the container widgetCursor.widget (not of an overlay)
static bool isScrollContainer(const ContainerWidget *widget, const Overlay *overlay) {
	return !overlay && widget->scroll == CONTAINER_WIDGET_SCROLL_VERTICAL;
}

// The height of the content of the scrolling container widgetCursor.widget: of
// a row or column its content size, else the bottom of the lowest child with
// the padding and the border of the container under it.
static int getScrollContentHeight(const WidgetCursor &widgetCursor) {
	auto widget = (const ContainerWidget *)widgetCursor.widget;
	if (isRowOrColumn(widget)) {
		int w, h;
		getLayoutContentSize(widgetCursor, w, h);
		return h;
	}
	int bottom = 0;
	auto &widgets = widget->widgets;
	for (uint32_t index = 0; index < widgets.count; ++index) {
		auto child = widgets[index];
		bottom = MAX(bottom, child->y + child->height);
	}
	const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));
	return bottom + style->paddingBottom + style->borderSizeBottom;
}

bool ContainerWidgetState::updateState() {
    WIDGET_STATE_START_EVEN_IF_HIDDEN(ContainerWidget);

	// the overlay data is updated also while the container is hidden
	if (widget->overlay == EEZ_DATA_ID_NONE) {
		WIDGET_STATE_RETURN_IF_HIDDEN();
	}

	WIDGET_STATE(styleId, overrideStyle(widgetCursor, widget->style));
	WIDGET_STATE(flags.active, g_isActiveWidget);
	WIDGET_STATE(flags.disabled, isWidgetActionDisabled(widgetCursor));

	overlay = getOverlay(widgetCursor);
	if (overlay) {
		// update overlay data
		auto containerWidget = (const ContainerWidget *)widget;
		Value widgetCursorValue((void *)&widgetCursor, VALUE_TYPE_POINTER);
		DATA_OPERATION_FUNCTION(containerWidget->overlay, DATA_OPERATION_UPDATE_OVERLAY_DATA, widgetCursor, widgetCursorValue);

		WIDGET_STATE(overlayState, overlay->state);
	}

	if (isScrollContainer(widget, overlay)) {
		// lines of 1 px (see ContainerWidgetState)
		count = MAX(getScrollContentHeight(widgetCursor), (int)widgetCursor.h);
		isVertical = true;
		lineExtent = 1;
		pitch = 1;
		itemsPerLine = 1;
		if (updateScrollState(widgetCursor, EEZ_DATA_ID_NONE, widgetCursor.h, hasPreviousState)) {
			hasPreviousState = false;
		}
	}

	WIDGET_STATE_END()
}

void ContainerWidgetState::render() {
	if (overlay && overlayState == 0) {
		return;
	}

    const WidgetCursor &widgetCursor = g_widgetCursor;

	displayBufferIndex = -1;
	if (overlay) {
		displayBufferIndex = display::beginBufferRendering();
	}

	auto stateStyle = getWidgetStateStyle(getStyle(styleId), flags.active, false, getDisabledStateFlag(flags.disabled));
	drawRectangle(
		widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
		stateStyle.style, stateStyle.active
	);

	repainted = true;

	ScrollableWidgetState::render();
}

void ContainerWidgetState::enumChildren() {
    if (overlay && overlayState == 0) {
        return;
    }

    WidgetCursor &widgetCursor = g_widgetCursor;
	auto widget = (const ContainerWidget *)widgetCursor.widget;

	auto stateStyle = getWidgetStateStyle(getStyle(styleId), flags.active, false, getDisabledStateFlag(flags.disabled));

	// A row or column is laid out before its children are enumerated. When the
	// layout is not the same as the last time (children moved, appeared,
	// changed size), the container is drawn again and so are its children.
	LayoutChild layout[LAYOUT_MAX_CHILDREN];
	int layoutCount = 0;
	bool rowOrColumn = !overlay && (widget->layout == CONTAINER_WIDGET_LAYOUT_HORIZONTAL || widget->layout == CONTAINER_WIDGET_LAYOUT_VERTICAL);
	bool scroll = isScrollContainer(widget, overlay);
	if (rowOrColumn) {
		uint32_t hash;
		// a scrolling row or column is laid out in the height of its content
		auto savedH = widgetCursor.h;
		if (scroll) {
			widgetCursor.h = count;
		}
		layoutCount = layoutChildren(widgetCursor, layout, hash);
		widgetCursor.h = savedH;
		if (g_findCallback == nullptr && hash != layoutHash) {
			layoutHash = hash;
			if (!repainted && !widgetCursor.refreshed) {
				auto savedOpacity = display::setOpacity(widgetCursor.opacity);
				drawRectangle(
					widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
					stateStyle.style, stateStyle.active
				);
				display::setOpacity(savedOpacity);
				repainted = true;
			}
		}
	}

	bool savedRefreshed = false;

	if (g_findCallback == nullptr) {
		if (overlay) {
			if (displayBufferIndex == -1) {
				displayBufferIndex = display::beginBufferRendering();
			}
		}

		savedRefreshed = widgetCursor.refreshed;
		if (repainted) {
			repainted = false;
			widgetCursor.refreshed = true;
		} else if (!widgetCursor.refreshed) {
			widgetCursor.pushBackground(widgetCursor.x, widgetCursor.y, stateStyle.style, stateStyle.active);
		}
	}

	// a scrolling container: clipped to the container, found as a scroll list
	// by the touch, drawn again when it is moved, the scroll indicator over the
	// children (as a List, see ScrollableWidgetState)
	EnumChildrenContext scrollContext;
	if (scroll) {
		beginEnumChildren(scrollContext);
	}

	// the children are inside of the rounded corners
	ChildrenRoundedClip roundedClip;
	roundedClip.begin(widgetCursor, overlay ? nullptr : stateStyle.style);

	auto savedWidget = widgetCursor.widget;

	// the children of a scrolling container are moved up by the scroll position
	auto yBeforeScroll = widgetCursor.y;
	if (scroll) {
		widgetCursor.y -= startPosition + scrollContext.renderPixelOffset;
	}

	int xOffset = 0;
	int yOffset = 0;

	if (overlay) {
		WidgetCursor &widgetCursor = g_widgetCursor;

		getOverlayOffset(widgetCursor, xOffset, yOffset);

		g_xOverlayOffset = xOffset;
		g_yOverlayOffset = yOffset;

		auto widget = (const ContainerWidget *)widgetCursor.widget;
		auto &widgets = widget->widgets;

		auto widgetOverrides = overlay->widgetOverrides;
		for (uint32_t index = 0; index < widgets.count; ++index) {
			if (widgetOverrides) {
				if (!widgetOverrides->isVisible) {
					widgetOverrides++;
					continue;
				}
			}

			widgetCursor.widget = widgets[index];

			int16_t xSaved = 0;
			int16_t ySaved = 0;
			int16_t wSaved = 0;
			int16_t hSaved = 0;

			if (widgetOverrides) {
				xSaved = widgetCursor.widget->x;
				ySaved = widgetCursor.widget->y;
				wSaved = widgetCursor.widget->width;
				hSaved = widgetCursor.widget->height;

				((Widget*)widgetCursor.widget)->x = widgetOverrides->x;
				((Widget*)widgetCursor.widget)->y = widgetOverrides->y;
				((Widget*)widgetCursor.widget)->width = widgetOverrides->w;
				((Widget*)widgetCursor.widget)->height = widgetOverrides->h;
			}

            auto savedX = widgetCursor.x;
            auto savedY = widgetCursor.y;

            widgetCursor.x += widgetCursor.widget->x;
            widgetCursor.y += widgetCursor.widget->y;

            widgetCursor.w = widgetCursor.widget->width;
            widgetCursor.h = widgetCursor.widget->height;

            enumWidget();

            widgetCursor.x = savedX;
            widgetCursor.y = savedY;

			if (widgetOverrides) {
				((Widget*)widgetCursor.widget)->x = xSaved;
				((Widget*)widgetCursor.widget)->y = ySaved;
				((Widget*)widgetCursor.widget)->width = wSaved;
				((Widget*)widgetCursor.widget)->height = hSaved;

				widgetOverrides++;
			}
		}

		g_xOverlayOffset = 0;
		g_yOverlayOffset = 0;
    } else {
		auto &widgets = widget->widgets;

        int containerOriginalWidth = widget->width;
        int containerOriginalHeight = widget->height;
        int containerWidth = widgetCursor.w;
        int containerHeight = widgetCursor.h;

        if (widget->layout == CONTAINER_WIDGET_LAYOUT_STATIC) {
            if (widget->flags & OVERLAPPING_CHILDREN_FLAG) {
                doStaticLayoutWithOverlapping(
                    widgetCursor, widgets,
                    containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight,
                    stateStyle.style, stateStyle.active
                );
            } else {
                doStaticLayout(widgetCursor, widgets, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);
            }
        } else if (rowOrColumn) {
            auto savedX = widgetCursor.x;
            auto savedY = widgetCursor.y;
            auto savedLayoutPlacedWidget = g_layoutPlacedWidget;

            for (uint32_t index = 0; index < widgets.count; ++index) {
                widgetCursor.widget = widgets[index];

                if ((int)index < layoutCount) {
                    auto &child = layout[index];
                    widgetCursor.x = savedX + child.x;
                    widgetCursor.y = savedY + child.y;
                    widgetCursor.w = child.w;
                    widgetCursor.h = child.h;
                } else {
                    // after LAYOUT_MAX_CHILDREN: not shown
                    widgetCursor.x = savedX;
                    widgetCursor.y = savedY;
                    widgetCursor.w = 0;
                    widgetCursor.h = 0;
                }

                g_layoutPlacedWidget = widgetCursor.widget;
                enumWidget();
            }

            g_layoutPlacedWidget = savedLayoutPlacedWidget;
            widgetCursor.x = savedX;
            widgetCursor.y = savedY;
        }
	}

	widgetCursor.widget = savedWidget;
	widgetCursor.y = yBeforeScroll;

	roundedClip.end();

	if (scroll) {
		endEnumChildren(scrollContext);
	}

	if (g_findCallback == nullptr) {
		if (overlay) {
			const Style *style = getStyle(widgetCursor.widget->style);

			display::endBufferRendering(
				displayBufferIndex,
				widgetCursor.x,
				widgetCursor.y,
				overlay ? overlay->width : widgetCursor.w,
				overlay ? overlay->height : widgetCursor.h,
				(widget->flags & SHADOW_FLAG) != 0,
				style->opacity,
				xOffset,
				yOffset,
				nullptr
			);

			displayBufferIndex = -1;
		}

		if (!widgetCursor.refreshed) {
			widgetCursor.popBackground();
		}

		widgetCursor.refreshed = savedRefreshed;
	}
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
