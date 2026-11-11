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

#include <eez/core/assets.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

namespace eez {
namespace flow {

#if defined(__EMSCRIPTEN__)
extern uint32_t g_wasmModuleId;
#endif

struct FlowState;

// 0 if the assets have no flows. External assets don't start the engine:
// their flows run while the flows of the main assets run.
unsigned start(Assets *assets);
void tick();
// stops the flows of the main assets, which stops all the flows
void stop();
// stops the flows of the assets; nullptr (e.g. assets that are already
// unloaded) does nothing. For external assets their flow states, at any level
// (e.g. a layout in a page of other assets, also the layouts a user widget
// keeps), are deleted at the end of the next tick.
void stop(Assets* assets);

bool isFlowStopped();
unsigned getTickMaxDurationCounter();

#if EEZ_OPTION_GUI
FlowState *getPageFlowState(Assets *assets, int16_t pageIndex, const WidgetCursor &widgetCursor);
#else
FlowState *getPageFlowState(Assets *assets, int16_t pageIndex);
#endif
int getPageIndex(FlowState *flowState);
int getPageIndexIncludeParents(FlowState *flowState);

void deletePageFlowState(Assets *assets, int16_t pageIndex);

Value getGlobalVariable(uint32_t globalVariableIndex);
Value getGlobalVariable(Assets *assets, uint32_t globalVariableIndex);
void setGlobalVariable(uint32_t globalVariableIndex, const Value &value);
void setGlobalVariable(Assets *assets, uint32_t globalVariableIndex, const Value &value);

// The user properties of the CallAction of the native action that is running.
// Outside a native action they do nothing; in a native action called without
// a CallAction (a widget event handler) they are a flow error.
Value getUserProperty(unsigned propertyIndex);
void setUserProperty(unsigned propertyIndex, const Value &value);

// Support for async execution for the native user actions. beginAsyncExecution
// returns nullptr outside a native action.
//
struct AsyncAction {
    // the flow state can be freed before the action ends (its flow stopped): the
    // functions below find it by flowStateId and do nothing if it is gone
    eez::flow::FlowState *flowState;
    unsigned componentIndex;
    uint32_t flowStateId;
};

// signal to the flow engine that async action has been started
AsyncAction *beginAsyncExecution();

// signal to the flow engine that async action has been ended
void endAsyncExecution(AsyncAction *asyncAction);

Value getUserPropertyAsync(AsyncAction *asyncAction, unsigned propertyIndex);
void setUserPropertyAsync(AsyncAction *asyncAction, unsigned propertyIndex, const Value &value);

#if EEZ_OPTION_GUI
FlowState *getUserWidgetFlowState(Assets *assets, FlowState *flowState, uint16_t userWidgetWidgetComponentIndex, int16_t pageIndex);
void executeFlowAction(const WidgetCursor &widgetCursor, int16_t actionId, void *param);
void dataOperation(int16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
int16_t getNativeVariableId(const WidgetCursor &widgetCursor);
#endif

void setDebuggerMessageSubsciptionFilter(uint32_t filter);
void onDebuggerClientConnected();
void onDebuggerClientDisconnected();

void onArrayValueFree(ArrayValue *arrayValue);
void onFreeMQTTConnection(ArrayValue *mqttConnectionValue);

void executeScpi();
void flushToDebuggerMessage();

} // flow
} // eez
