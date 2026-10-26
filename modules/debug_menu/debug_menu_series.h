/**************************************************************************/
/*  debug_menu_series.h                                                   */
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

#include "core/templates/vector.h"

/// Fixed-size ring buffer of frame values with the statistics the debug menu needs.
/// Pure C++ so it can be unit tested without a scene tree.
class DebugMenuSeries {
	Vector<float> samples;
	int capacity = 0;
	int write_index = 0;
	int count = 0;
	double sum = 0.0;

public:
	void resize(int p_capacity);
	void clear();
	void fill(float p_value);
	void push(float p_value);

	int size() const { return count; }
	int get_capacity() const { return capacity; }
	bool is_empty() const { return count == 0; }

	/// `p_index` 0 is the oldest retained sample.
	float get_at(int p_index) const;
	float get_last() const;
	float get_avg() const;
	float get_min() const;
	float get_max() const;

	/// Nearest-rank percentile, `p_fraction` in [0, 1]. 0.99 returns the value at
	/// the 99th percentile of frame times (used for 1% low frame rates).
	float get_percentile(float p_fraction) const;
};
