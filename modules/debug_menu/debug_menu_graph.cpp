/**************************************************************************/
/*  debug_menu_graph.cpp                                                  */
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

#include "debug_menu_graph.h"

#include "debug_menu_series.h"
#include "debug_menu_util.h"

DebugMenuGraph::DebugMenuGraph() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	set_custom_minimum_size(Vector2(DebugMenuUtil::GRAPH_WIDTH, DebugMenuUtil::GRAPH_HEIGHT));
	set_v_size_flags(SIZE_SHRINK_BEGIN);
}

void DebugMenuGraph::_notification(int p_what) {
	if (p_what != NOTIFICATION_DRAW) {
		return;
	}

	// Equivalent to the add-on's default StyleBoxFlat: flat black, no border, no radius.
	draw_rect(Rect2(Point2(), get_size()), Color(0, 0, 0, 0.25098), true);

	if (!series || series->size() < 2) {
		return;
	}

	const int count = series->size();
	PackedVector2Array points;
	points.resize(count);

	for (int i = 0; i < count; i++) {
		const double x = DebugMenuUtil::graph_x(i, count);
		const double value = series->get_at(i);
		const double y = scale_mode == SCALE_FPS
				? DebugMenuUtil::graph_y_fps(value)
				: DebugMenuUtil::graph_y_frametime(value, legacy_frametime_scale);
		points.write[i] = Point2((float)x, (float)y);
	}

	// No antialiasing, 1 px wide, exactly like the add-on. A negative width selects a
	// line strip (one vertex per sample) instead of a quad strip, which keeps the same
	// 1 px look but costs roughly half the vertex work in the canvas item.
	draw_polyline(points, line_color, -1.0);
}
