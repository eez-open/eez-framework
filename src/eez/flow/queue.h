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

#include <eez/flow/private.h>

namespace eez {
namespace flow {

void queueReset();
size_t getQueueSize();
size_t getMaxQueueSize();
extern unsigned g_numNonContinuousTaskInQueue;
bool addToQueue(FlowState *flowState, unsigned componentIndex,
    int sourceComponentIndex, int sourceOutputIndex, int targetInputIndex,
    bool continuousTask);
bool peekNextTaskFromQueue(FlowState *&flowState, unsigned &componentIndex, bool &continuousTask);
void removeNextTaskFromQueue();

bool isInQueue(FlowState *flowState, unsigned componentIndex);

void removeTasksFromQueueForFlowState(FlowState *flowState);

} // flow
} // eez
