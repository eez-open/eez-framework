/*
 * eez-framework tests
 *
 * The configuration of the BB3+ (src/bb3/conf/eez-framework-conf.h) on the CM5
 * platform (Linux, SDL). The GUI loop runs on the process main thread, so the
 * tests drive it frame by frame (harness.h).
 */

#pragma once

#include <stdint.h>

static const uint32_t DISPLAY_WIDTH = 800;
static const uint32_t DISPLAY_HEIGHT = 480;

// RGBA8888
#define DISPLAY_BPP 32

static const char *TITLE = "eez-framework tests";
static const char *ICON = "eez.png";

#define MAX_NUM_OF_Y_AXES 18
#define MAX_NUM_OF_Y_VALUES MAX_NUM_OF_Y_AXES

#define OPTION_KEYBOARD 1
#define OPTION_KEYPAD 1
#define UTF8_SUPPORT 1

#define EEZ_OPTION_GUI_VECTOR_FONTS 1

#define DISPLAY_BACKGROUND_LUMINOSITY_STEP_MIN 0
#define DISPLAY_BACKGROUND_LUMINOSITY_STEP_MAX 20
#define DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT 10

static const uint32_t GUI_STATE_BUFFER_SIZE = 128 * 1024;

#define OPTION_TOUCH_CALIBRATION 0
#define SKIP_TOUCH_CALIBRATION
#define EEZ_CUSTOM_TOUCH_MEASURE_CALLBACK

#define SHADOW_GLYPH_TOP_PX 15
#define SHADOW_GLYPH_RIGHT_PX 25
#define SHADOW_GLYPH_BOTTOM_PX 25
#define SHADOW_GLYPH_LEFT_PX 15
#define SHADOW_GLYPH_WIDTH_PX 40
#define SHADOW_GLYPH_HEIGHT_PX 40

#define EEZ_OPTION_GUI 1

// BB3+ has OPTION_SCPI 1: it only adds the SCPI unit mapping in core/unit.cpp
// and needs libscpi, so the tests leave it out
#define OPTION_SCPI 0

#define MIN_TOUCH_DETECTION_SIZE 60

// values on the expression evaluation stack (eez/flow/expression.h), the
// properties of nested user widgets are evaluated on the same stack
#define EEZ_FLOW_EVAL_STACK_SIZE 64

// the GUI "thread" is the process main thread (gui/thread.cpp), so isGuiThread()
// is true in the tests and the harness processes the GUI queue itself
#define EEZ_GUI_RUNS_ON_MAIN_THREAD 1
