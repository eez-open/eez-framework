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

enum EventType {
    EVENT_TYPE_TOUCH_NONE,
    EVENT_TYPE_TOUCH_DOWN,
    EVENT_TYPE_TOUCH_MOVE,
    EVENT_TYPE_TOUCH_UP,

    EVENT_TYPE_LONG_TOUCH,
    EVENT_TYPE_EXTRA_LONG_TOUCH,
    EVENT_TYPE_AUTO_REPEAT
};

struct Event {
    uint32_t time;
    EventType type;
    int x;
    int y;
};

void onTouchEvent(bool pressed, int x, int y, Event &lastEvent);
void processTouchEvent(Event &event);

extern bool g_isLongTouch;

} // namespace gui
} // namespace eez
