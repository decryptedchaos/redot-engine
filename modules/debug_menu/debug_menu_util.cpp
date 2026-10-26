/**************************************************************************/
/*  debug_menu_util.cpp                                                   */
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

#include "debug_menu_util.h"

#include "core/math/math_funcs.h"

namespace DebugMenuUtil {

struct GradientStop {
	float offset = 0.0f;
	Color color;
};

// Same stops and order as the add-on's Gradient (tailwindcolor.com).
static const GradientStop FRAME_TIME_STOPS[4] = {
	{ 0.0f, Color::from_rgba8(239, 68, 68) }, // red-500
	{ 0.3333f, Color::from_rgba8(250, 204, 21) }, // yellow-400
	{ 0.6667f, Color::from_rgba8(128, 226, 95) }, // 50-50 mix of lime-400 and green-400
	{ 1.0f, Color::from_rgba8(56, 189, 248) }, // light-blue-400
};

String format_ms(double p_value) {
	return String::num(p_value, 2).pad_decimals(2);
}

String format_fps(double p_fps) {
	return String::num_int64((int64_t)Math::floor(p_fps)) + " FPS";
}

Color frame_time_color(double p_fps) {
	const double t = remap_clamped(p_fps, GRAPH_MIN_FPS, GRAPH_MAX_FPS, 0.0, 1.0);

	if (t <= FRAME_TIME_STOPS[0].offset) {
		return FRAME_TIME_STOPS[0].color;
	}
	for (int i = 1; i < 4; i++) {
		if (t <= FRAME_TIME_STOPS[i].offset) {
			const GradientStop &a = FRAME_TIME_STOPS[i - 1];
			const GradientStop &b = FRAME_TIME_STOPS[i];
			const double span = (double)b.offset - (double)a.offset;
			const double weight = span > 0.0 ? (t - (double)a.offset) / span : 0.0;
			return a.color.lerp(b.color, weight);
		}
	}
	return FRAME_TIME_STOPS[3].color;
}

double remap_clamped(double p_value, double p_in_min, double p_in_max, double p_out_min, double p_out_max) {
	if (p_in_max == p_in_min) {
		return p_out_min;
	}
	const double clamped = CLAMP(p_value, p_in_min, p_in_max);
	return p_out_min + (clamped - p_in_min) * (p_out_max - p_out_min) / (p_in_max - p_in_min);
}

double graph_x(int p_index, int p_count) {
	if (p_count <= 0) {
		return 0.0;
	}
	return Math::remap((double)p_index, 0.0, (double)p_count, 0.0, (double)GRAPH_WIDTH);
}

double graph_y_fps(double p_fps) {
	return remap_clamped(p_fps, GRAPH_MIN_FPS, GRAPH_MAX_FPS, GRAPH_HEIGHT, 0.0);
}

double graph_y_frametime(double p_frametime_ms, bool p_legacy_scale) {
	if (p_legacy_scale) {
		// Reproduces the add-on's clamp of millisecond values to FPS bounds.
		return remap_clamped(p_frametime_ms, GRAPH_MIN_FPS, GRAPH_MAX_FPS, GRAPH_HEIGHT, 0.0);
	}

	// D-1: convert to FPS first so slow frames go up, matching the FPS graph logic.
	const double fps = p_frametime_ms > 0.0 ? 1000.0 / p_frametime_ms : GRAPH_MAX_FPS;
	return graph_y_fps(fps);
}

String build_cap_suffix(int p_max_fps, bool p_low_processor_mode, int p_low_processor_sleep_usec, int p_vsync_mode) {
	String vsync_string;
	switch (p_vsync_mode) {
		case VSYNC_ENABLED:
			vsync_string = "V-Sync";
			break;
		case VSYNC_ADAPTIVE:
			vsync_string = "Adaptive V-Sync";
			break;
		case VSYNC_MAILBOX:
			vsync_string = "Mailbox V-Sync";
			break;
		default:
			break;
	}

	bool has_cap = false;
	int fps_cap = 0;

	if (p_low_processor_mode) {
		// D-3: only consider the low-processor cap when that mode is actually enabled.
		const int sleep_usec = MAX(1, p_low_processor_sleep_usec);
		fps_cap = MAX(1, (int)Math::round(1000000.0 / (double)sleep_usec));
		has_cap = true;
	}

	if (p_max_fps > 0) {
		fps_cap = has_cap ? MIN(p_max_fps, fps_cap) : p_max_fps;
		has_cap = true;
	}

	if (has_cap) {
		String suffix = " (cap: " + itos(fps_cap) + " FPS";
		if (!vsync_string.is_empty()) {
			suffix += " + " + vsync_string;
		}
		suffix += ")";
		return suffix;
	}

	if (!vsync_string.is_empty()) {
		return " (" + vsync_string + ")";
	}

	return String();
}

String unicode_times() {
	return String::chr(0x00D7);
}

String unicode_arrow_up() {
	return String::chr(0x2191);
}

String unicode_arrow_down() {
	return String::chr(0x2193);
}

String uint64_to_string(uint64_t p_value) {
	return String::num_int64((int64_t)p_value);
}

} // namespace DebugMenuUtil
