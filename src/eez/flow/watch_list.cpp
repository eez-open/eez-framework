/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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

#include <eez/flow/watch_list.h>
#include <eez/flow/components.h>

namespace eez {
namespace flow {

void executeWatchVariableComponent(FlowState *flowState, unsigned componentIndex);

struct WatchListNode {
    FlowState *flowState;
    unsigned componentIndex;

    WatchListNode *prev;
    WatchListNode *next;
};

struct WatchList {
    WatchListNode *first;
    WatchListNode *last;
};

static WatchList g_watchList;

WatchListNode *watchListAdd(FlowState *flowState, unsigned componentIndex) {
    auto node = (WatchListNode *)alloc(sizeof(WatchListNode), 0x00864d67);

    node->prev = g_watchList.last;
    if (g_watchList.last != 0) {
        g_watchList.last->next = node;
    }
    g_watchList.last = node;

    if (g_watchList.first == 0) {
        g_watchList.first = node;
    }

    node->next = 0;

    node->flowState = flowState;
    node->componentIndex = componentIndex;

    flowState->numWatchComponents++;

    return node;
}

void watchListRemove(WatchListNode *node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        g_watchList.first = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        g_watchList.last = node->prev;
    }

    node->flowState->numWatchComponents--;

    free(node);
}

void visitWatchList() {
    for (auto node = g_watchList.first; node; node = node->next) {
        executeWatchVariableComponent(node->flowState, node->componentIndex);

        if (canFreeFlowState(node->flowState, false)) {
            deallocateComponentExecutionState(node->flowState, node->componentIndex);

            auto nextNode = node->next;
            watchListRemove(node);
            node = nextNode;
            if (!node) {
                break;
            }
        }
    }
}

void watchListReset() {
    for (auto node = g_watchList.first; node;) {
        auto nextNode = node->next;
        watchListRemove(node);
        node = nextNode;
    }
}

} // namespace flow
} // namespace eez
