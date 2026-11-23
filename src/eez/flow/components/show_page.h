/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <eez/flow/private.h>

namespace eez {
namespace flow {

// how ShowPage changes the page stack
enum ShowPageMode {
    SHOW_PAGE_MODE_REPLACE = 0, // the page replaces the active page (replacePageHook)
    SHOW_PAGE_MODE_PUSH = 1,    // the page goes on the page stack (pushPageHook)
    SHOW_PAGE_MODE_SET = 2      // the page stack is cleared, the page is the only one (setPageHook)
};

struct ShowPageActionComponent : public Component {
    int16_t page;
    int16_t mode; // ShowPageMode
};

} // flow
} // eez
