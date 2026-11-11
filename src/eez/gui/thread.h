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

#include <eez/core/os.h>

#include <eez/gui/event.h>

namespace eez {
namespace gui {

#if EEZ_OPTION_THREADS
void startThread();
#endif

#if EEZ_OPTION_THREADS && defined(EEZ_PLATFORM_SIMULATOR) && defined(__APPLE__)
// On macOS, Cocoa (SDL video) must run on the process main thread, so the GUI
// loop isn't started in its own thread. Instead, main() calls this function,
// which waits for startThread() and then runs the GUI loop until shutdown.
#define EEZ_GUI_RUNS_ON_MAIN_THREAD 1
void runOnMainThread();
#endif

enum {
    GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC = 1,

    GUI_QUEUE_MESSAGE_TYPE_TOUCH_EVENT,

    GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE,
    GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE,

    GUI_QUEUE_MESSAGE_REFRESH_SCREEN,

    GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_CONNECTED,
    GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED,
    GUI_QUEUE_MESSAGE_DEBUGGER_INPUT_AVAILABLE,

    GUI_QUEUE_MESSAGE_KEY_DOWN,

    // runs a function with a parameter on the GUI thread (callInGuiThread)
    GUI_QUEUE_MESSAGE_TYPE_CALL,
};

class AppContext;
class Page;

void sendMessageToGuiThread(uint8_t messageType, uint32_t messageParam = 0, uint32_t timeoutMillisec = 0xFFFFFFFF);
void sendTouchEventToGuiThread(Event &touchEvent);
bool pushPageInGuiThread(AppContext *appContext, int pageId, Page *page);
bool showPageInGuiThread(AppContext *appContext, int pageId);

// Called on another thread: queues func(param) for the GUI thread and returns true.
// Called on the GUI thread: returns false and does nothing (the caller does the
// work itself).
bool callInGuiThread(void (*func)(void *param), void *param);

bool isGuiThread();

void processGuiQueue(uint32_t timeoutMillisec);

void suspendGuiThread();
void resumeGuiThread();

} // namespace gui
} // namespace eez
