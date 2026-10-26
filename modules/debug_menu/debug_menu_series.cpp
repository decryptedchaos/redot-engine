/**************************************************************************/
/*  debug_menu_series.cpp                                                 */
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

#include "debug_menu_series.h"

#include "core/math/math_funcs.h"
#include "core/templates/vector.h"

void DebugMenuSeries::resize(int p_capacity) {
	capacity = MAX(1, p_capacity);
	samples.resize(capacity);
	samples.fill(0.0f);
	write_index = 0;
	count = 0;
	sum = 0.0;
}

void DebugMenuSeries::clear() {
	if (capacity == 0) {
		return;
	}
	samples.fill(0.0f);
	write_index = 0;
	count = 0;
	sum = 0.0;
}

void DebugMenuSeries::fill(float p_value) {
	for (int i = 0; i < capacity; i++) {
		samples.write[i] = p_value;
	}
	write_index = 0;
	count = capacity;
	sum = (double)p_value * (double)capacity;
}

void DebugMenuSeries::push(float p_value) {
	if (capacity == 0) {
		return;
	}

	if (count == capacity) {
		sum -= (double)samples[write_index];
	} else {
		count++;
	}

	samples.write[write_index] = p_value;
	sum += (double)p_value;
	write_index = (write_index + 1) % capacity;
}

float DebugMenuSeries::get_at(int p_index) const {
	if (count == 0 || capacity == 0) {
		return 0.0f;
	}
	const int clamped = CLAMP(p_index, 0, count - 1);
	const int start = (write_index - count + capacity) % capacity;
	return samples[(start + clamped) % capacity];
}

float DebugMenuSeries::get_last() const {
	if (count == 0) {
		return 0.0f;
	}
	const int index = (write_index - 1 + capacity) % capacity;
	return samples[index];
}

float DebugMenuSeries::get_avg() const {
	if (count == 0) {
		return 0.0f;
	}
	return (float)(sum / (double)count);
}

float DebugMenuSeries::get_min() const {
	if (count == 0) {
		return 0.0f;
	}
	float minimum = get_at(0);
	for (int i = 1; i < count; i++) {
		minimum = MIN(minimum, get_at(i));
	}
	return minimum;
}

float DebugMenuSeries::get_max() const {
	if (count == 0) {
		return 0.0f;
	}
	float maximum = get_at(0);
	for (int i = 1; i < count; i++) {
		maximum = MAX(maximum, get_at(i));
	}
	return maximum;
}

float DebugMenuSeries::get_percentile(float p_fraction) const {
	if (count == 0) {
		return 0.0f;
	}

	Vector<float> sorted;
	sorted.resize(count);
	for (int i = 0; i < count; i++) {
		sorted.write[i] = get_at(i);
	}
	sorted.sort();

	const double fraction = CLAMP((double)p_fraction, 0.0, 1.0);
	// Nearest rank. The epsilon keeps float rounding (0.99f * 100) from bumping the rank
	// up by a whole sample.
	int rank = (int)Math::ceil(fraction * (double)count - 1e-6) - 1;
	rank = CLAMP(rank, 0, count - 1);
	return sorted[rank];
}
