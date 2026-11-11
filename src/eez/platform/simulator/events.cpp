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

#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__) || defined(EEZ_PLATFORM_CM5_RASPI) 

#if EEZ_OPTION_GUI

#include <math.h>

#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
#include <SDL.h>
#endif


#include <eez/core/encoder.h>
#include <eez/core/keyboard.h>
#include <eez/core/os.h>

#include <eez/gui/gui.h>

#include <eez/platform/simulator/events.h>

namespace eez {

namespace platform {
namespace simulator {

int g_mouseX;
int g_mouseY;
bool g_mouseButton1IsPressed;

static int g_simulatedTouchX;
static int g_simulatedTouchY;
static int g_simulatedTouchEndX;
static int g_simulatedTouchEndY;
static uint32_t g_simulatedTouchStart;
static uint32_t g_simulatedTouchDuration;
static volatile bool g_simulatedTouch;

void simulateTouch(int x, int y, uint32_t durationMs) {
    simulateDrag(x, y, x, y, durationMs);
}

void simulateDrag(int x1, int y1, int x2, int y2, uint32_t durationMs) {
    g_simulatedTouchX = x1;
    g_simulatedTouchY = y1;
    g_simulatedTouchEndX = x2;
    g_simulatedTouchEndY = y2;
    g_simulatedTouchStart = millis();
    g_simulatedTouchDuration = durationMs;
    g_simulatedTouch = true;
}

void readEvents() {
#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
    int yMouseWheel = 0;
    bool mouseButton2IsUp = false;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == 1) {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    g_mouseButton1IsPressed = true;
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    g_mouseButton1IsPressed = false;
                }
            }
            if (event.button.button == 2) {
                if (event.type == SDL_MOUSEBUTTONUP) {
                    mouseButton2IsUp = true;
                }
            }
        } else if (event.type == SDL_MOUSEWHEEL) {
            // A wheel notch is an encoder detent, the wheel turned away from
            // the user is the encoder turned clockwise. The notches are
            // written once per call, i.e. once per display frame as the
            // SYSMCU frames on the hardware, the encoder measures the speed.
            yMouseWheel += event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
        } else if (event.type == SDL_KEYDOWN) {
            keyboard::onKeyboardEvent(&event.key);
        } else if (event.type == SDL_WINDOWEVENT)  {
            if (event.window.event == SDL_WINDOWEVENT_SHOWN || event.window.event == SDL_WINDOWEVENT_RESTORED) {
                eez::gui::refreshScreen();
            }
        }

        if (event.type == SDL_QUIT) {
            eez::shutdown();
        }
    }

    SDL_GetMouseState(&g_mouseX, &g_mouseY);

    // simulated touch: pressed for the given duration (moving from the start
    // to the end point), then released
    if (g_simulatedTouch) {
        uint32_t elapsed = millis() - g_simulatedTouchStart;
        float t = g_simulatedTouchDuration > 0 && elapsed < g_simulatedTouchDuration ? (float)elapsed / g_simulatedTouchDuration : 1.0f;
        g_mouseX = g_simulatedTouchX + (int)((g_simulatedTouchEndX - g_simulatedTouchX) * t);
        g_mouseY = g_simulatedTouchY + (int)((g_simulatedTouchEndY - g_simulatedTouchY) * t);
        g_mouseButton1IsPressed = elapsed < g_simulatedTouchDuration;
        if (!g_mouseButton1IsPressed) {
            g_simulatedTouch = false;
        }
    }

    // for web simulator
    if (yMouseWheel >= 100 || yMouseWheel <= -100) {
        yMouseWheel /= 100;
    }

#if OPTION_ENCODER
    platform::encoder::write(yMouseWheel, mouseButton2IsUp);
#endif
#endif
}

bool isMiddleButtonPressed() {
#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
    int x;
    int y;
    auto buttons = SDL_GetMouseState(&x, &y);
    return buttons & SDL_BUTTON(2) ? true : false;
#else
    return false;
#endif
}

} // namespace simulator
} // namespace platform
} // namespace eez

#if defined(__EMSCRIPTEN__)

EM_PORT_API(void) onPointerEvent(int x, int y, int pressed) {
    using namespace eez::platform::simulator;

    g_mouseX = x;
    g_mouseY = y;
    g_mouseButton1IsPressed = pressed;
}

EM_PORT_API(void) onMouseWheelEvent(double yMouseWheel, int clicked) {
#if OPTION_ENCODER
    if (yMouseWheel >= 100 || yMouseWheel <= -100) {
        yMouseWheel /= 100;
    }
    eez::platform::encoder::write(round(yMouseWheel), clicked);
#endif
}

#endif

#endif

#endif // defined(EEZ_PLATFORM_SIMULATOR)
