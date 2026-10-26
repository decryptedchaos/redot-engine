/**************************************************************************/
/*  test_debug_menu_util.h                                                */
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

#include "modules/debug_menu/debug_menu_util.h"

#include "scene/resources/gradient.h"

#include "tests/test_macros.h"

#include <cmath>

namespace TestDebugMenuUtil {

TEST_CASE("[DebugMenu][Util] Frame time formatting") {
	CHECK_EQ(DebugMenuUtil::format_ms(0.0), "0.00");
	CHECK_EQ(DebugMenuUtil::format_ms(1.2), "1.20");
	CHECK_EQ(DebugMenuUtil::format_ms(16.666666), "16.67");
	CHECK_EQ(DebugMenuUtil::format_ms(123.456), "123.46");
	CHECK_EQ(DebugMenuUtil::format_fps(59.9), "59 FPS");
	CHECK_EQ(DebugMenuUtil::format_fps(60.0), "60 FPS");
}

TEST_CASE("[DebugMenu][Util] Gradient matches a Gradient resource") {
	Ref<Gradient> gradient;
	gradient.instantiate();
	gradient->set_color(0, Color::from_rgba8(239, 68, 68));
	gradient->set_color(1, Color::from_rgba8(56, 189, 248));
	gradient->add_point(0.3333f, Color::from_rgba8(250, 204, 21));
	gradient->add_point(0.6667f, Color::from_rgba8(128, 226, 95));

	const double samples[] = { 0.0, 10.0, 30.0, 60.0, 110.0, 160.0, 240.0 };
	for (double fps : samples) {
		const double t = DebugMenuUtil::remap_clamped(fps, DebugMenuUtil::GRAPH_MIN_FPS, DebugMenuUtil::GRAPH_MAX_FPS, 0.0, 1.0);
		const Color expected = gradient->get_color_at_offset(t);
		const Color actual = DebugMenuUtil::frame_time_color(fps);

		CHECK_MESSAGE(actual.r == doctest::Approx(expected.r), "red mismatch at ", fps, " FPS");
		CHECK_MESSAGE(actual.g == doctest::Approx(expected.g), "green mismatch at ", fps, " FPS");
		CHECK_MESSAGE(actual.b == doctest::Approx(expected.b), "blue mismatch at ", fps, " FPS");
		CHECK_MESSAGE(actual.a == doctest::Approx(expected.a), "alpha mismatch at ", fps, " FPS");
	}
}

TEST_CASE("[DebugMenu][Util] Graph mapping") {
	CHECK_EQ(DebugMenuUtil::graph_x(0, 150), doctest::Approx(0.0));
	CHECK_EQ(DebugMenuUtil::graph_x(75, 150), doctest::Approx(75.0));
	CHECK_EQ(DebugMenuUtil::graph_x(149, 150), doctest::Approx(149.0));

	CHECK_EQ(DebugMenuUtil::graph_y_fps(10.0), doctest::Approx((double)DebugMenuUtil::GRAPH_HEIGHT));
	CHECK_EQ(DebugMenuUtil::graph_y_fps(160.0), doctest::Approx(0.0));
	CHECK_EQ(DebugMenuUtil::graph_y_fps(60.0), doctest::Approx(16.6666666));

	// D-1: converting 16.67 ms to 60 FPS puts the line in the middle, not at the bottom.
	const double fixed_y = DebugMenuUtil::graph_y_frametime(16.666666, false);
	const double legacy_y = DebugMenuUtil::graph_y_frametime(16.666666, true);
	CHECK(fixed_y < legacy_y);
	CHECK_EQ(fixed_y, doctest::Approx(16.6666666));
	CHECK_EQ(legacy_y, doctest::Approx(23.888888));

	// Zero or negative frame times must not produce infinity.
	CHECK(std::isfinite(DebugMenuUtil::graph_y_frametime(0.0, false)));
	CHECK(std::isfinite(DebugMenuUtil::graph_y_frametime(-1.0, false)));
}

TEST_CASE("[DebugMenu][Util] FPS cap suffix") {
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(0, false, 10000, DebugMenuUtil::VSYNC_DISABLED), "");
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(60, false, 10000, DebugMenuUtil::VSYNC_DISABLED), " (cap: 60 FPS)");
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(60, false, 10000, DebugMenuUtil::VSYNC_ENABLED), " (cap: 60 FPS + V-Sync)");
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(0, false, 10000, DebugMenuUtil::VSYNC_ADAPTIVE), " (Adaptive V-Sync)");
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(0, false, 10000, DebugMenuUtil::VSYNC_MAILBOX), " (Mailbox V-Sync)");

	// D-3: low-processor mode is off, so only Engine.max_fps applies.
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(144, false, 10000, DebugMenuUtil::VSYNC_DISABLED), " (cap: 144 FPS)");

	// Low-processor mode is on: the lower of the two caps wins.
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(144, true, 10000, DebugMenuUtil::VSYNC_DISABLED), " (cap: 100 FPS)");
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(0, true, 5000, DebugMenuUtil::VSYNC_DISABLED), " (cap: 200 FPS)");
	CHECK_EQ(DebugMenuUtil::build_cap_suffix(0, true, 1000000, DebugMenuUtil::VSYNC_DISABLED), " (cap: 1 FPS)");

	// Guard against a zero sleep duration.
	const String zero_sleep = DebugMenuUtil::build_cap_suffix(0, true, 0, DebugMenuUtil::VSYNC_DISABLED);
	CHECK(!zero_sleep.is_empty());
}

TEST_CASE("[DebugMenu][Util] Unicode and counters") {
	CHECK_EQ(DebugMenuUtil::unicode_times(), String::chr(0x00D7));
	CHECK_EQ(DebugMenuUtil::unicode_arrow_up(), String::chr(0x2191));
	CHECK_EQ(DebugMenuUtil::unicode_arrow_down(), String::chr(0x2193));
	CHECK_EQ(DebugMenuUtil::uint64_to_string(1234567890123ULL), "1234567890123");
}

} // namespace TestDebugMenuUtil
