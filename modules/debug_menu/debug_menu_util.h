/**************************************************************************/
/*  debug_menu_util.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/math/color.h"
#include "core/string/ustring.h"

namespace DebugMenuUtil {

// Keep these in sync with the add-on's constants.
constexpr int GRAPH_WIDTH = 150;
constexpr int GRAPH_HEIGHT = 25;
constexpr float GRAPH_MIN_FPS = 10.0f;
constexpr float GRAPH_MAX_FPS = 160.0f;
constexpr float GRAPH_MIN_FRAMETIME = 1.0f / GRAPH_MIN_FPS;
constexpr float GRAPH_MAX_FRAMETIME = 1.0f / GRAPH_MAX_FPS;
constexpr int DEFAULT_HISTORY_FRAMES = 150;

// Mirror DisplayServer::VSyncMode values so the helpers stay dependency free and testable.
constexpr int VSYNC_DISABLED = 0;
constexpr int VSYNC_ENABLED = 1;
constexpr int VSYNC_ADAPTIVE = 2;
constexpr int VSYNC_MAILBOX = 3;

/// Formats a value the way `str(value).pad_decimals(2)` does in GDScript.
String format_ms(double p_value);

/// Formats a frame rate the way the add-on does (`floor(fps) + " FPS"`).
String format_fps(double p_fps);

/// Samples the shared red/yellow/green/cyan gradient for a frame rate.
Color frame_time_color(double p_fps);

/// Remaps and clamps a value.
double remap_clamped(double p_value, double p_in_min, double p_in_max, double p_out_min, double p_out_max);

/// Horizontal graph position for sample `p_index` of `p_count` (matches the add-on's remap).
double graph_x(int p_index, int p_count);

/// Vertical graph position for a frame rate, using the add-on's FPS scale.
double graph_y_fps(double p_fps);

/// Vertical graph position for a frame time in milliseconds.
/// `p_legacy_scale` reproduces the add-on's original (flat) clamp for A/B comparison.
double graph_y_frametime(double p_frametime_ms, bool p_legacy_scale);

/// Builds the " (cap: … + V-Sync)" suffix shown after the frame time.
String build_cap_suffix(int p_max_fps, bool p_low_processor_mode, int p_low_processor_sleep_usec, int p_vsync_mode);

/// "×" without depending on the compiler execution charset.
String unicode_times();

/// "↑" and "↓".
String unicode_arrow_up();
String unicode_arrow_down();

/// Stable unsigned 64-bit formatting for the frame counter and CSV output.
String uint64_to_string(uint64_t p_value);

} // namespace DebugMenuUtil
