/**************************************************************************/
/*  test_debug_menu_series.h                                              */
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

#include "modules/debug_menu/debug_menu_series.h"

#include "tests/test_macros.h"

namespace TestDebugMenuSeries {

TEST_CASE("[DebugMenu][Series] Ring buffer statistics") {
	DebugMenuSeries series;
	series.resize(5);

	CHECK(series.is_empty());
	CHECK_EQ(series.get_capacity(), 5);
	CHECK_EQ(series.get_avg(), doctest::Approx(0.0));

	for (int i = 1; i <= 7; i++) {
		series.push((float)i);
	}

	CHECK_EQ(series.size(), 5);
	CHECK_EQ(series.get_at(0), doctest::Approx(3.0));
	CHECK_EQ(series.get_at(4), doctest::Approx(7.0));
	CHECK_EQ(series.get_avg(), doctest::Approx(5.0));
	CHECK_EQ(series.get_min(), doctest::Approx(3.0));
	CHECK_EQ(series.get_max(), doctest::Approx(7.0));
	CHECK_EQ(series.get_last(), doctest::Approx(7.0));

	series.fill(2.5f);
	CHECK_EQ(series.size(), 5);
	CHECK_EQ(series.get_avg(), doctest::Approx(2.5));
	CHECK_EQ(series.get_min(), doctest::Approx(2.5));
	CHECK_EQ(series.get_max(), doctest::Approx(2.5));

	series.clear();
	CHECK(series.is_empty());
	CHECK_EQ(series.get_avg(), doctest::Approx(0.0));
	CHECK_EQ(series.get_last(), doctest::Approx(0.0));
}

TEST_CASE("[DebugMenu][Series] Percentiles") {
	DebugMenuSeries series;
	series.resize(100);
	for (int i = 1; i <= 100; i++) {
		series.push((float)i);
	}

	CHECK_EQ(series.get_percentile(0.0f), doctest::Approx(1.0));
	CHECK_EQ(series.get_percentile(0.5f), doctest::Approx(50.0));
	CHECK_EQ(series.get_percentile(0.99f), doctest::Approx(99.0));
	CHECK_EQ(series.get_percentile(0.999f), doctest::Approx(100.0));
	CHECK_EQ(series.get_percentile(1.0f), doctest::Approx(100.0));
}

TEST_CASE("[DebugMenu][Series] Resize clears") {
	DebugMenuSeries series;
	series.resize(4);
	series.push(1.0f);
	series.push(2.0f);
	CHECK_EQ(series.size(), 2);

	series.resize(8);
	CHECK_EQ(series.get_capacity(), 8);
	CHECK(series.is_empty());
	CHECK_EQ(series.get_avg(), doctest::Approx(0.0));
}

} // namespace TestDebugMenuSeries
