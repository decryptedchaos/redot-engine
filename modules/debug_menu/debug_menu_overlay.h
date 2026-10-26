/**************************************************************************/
/*  debug_menu_overlay.h                                                  */
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

#include "debug_menu.h"
#include "debug_menu_recorder.h"
#include "debug_menu_series.h"
#include "debug_menu_util.h"

#include "core/object/worker_thread_pool.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "scene/main/canvas_layer.h"

class Control;
class VBoxContainer;
class GridContainer;
class HBoxContainer;
class Label;
class DebugMenuGraph;
class InputEvent;

/// Native replacement for the add-on's `debug_menu.tscn` + `debug_menu.gd`.
/// The whole node tree is built in C++ so the overlay has no scene or script dependency.
class DebugMenuOverlay : public CanvasLayer {
	GDCLASS(DebugMenuOverlay, CanvasLayer);

public:
	enum GraphIndex {
		GRAPH_FPS,
		GRAPH_TOTAL,
		GRAPH_CPU,
		GRAPH_GPU,
	};

private:
	// 12 history cells + frame number + FPS + frame time + percentiles.
	static constexpr int DETAIL_SLOT_COUNT = 16;

	DebugMenu::Style style = DebugMenu::HIDDEN;

	Control *menu_control = nullptr;
	VBoxContainer *vbox = nullptr;
	Label *fps_label = nullptr;
	Label *frame_time_label = nullptr;
	Label *frame_number_label = nullptr;
	GridContainer *history_grid = nullptr;
	Label *history_value_labels[3][4] = {};
	HBoxContainer *graph_rows[4] = {};
	DebugMenuGraph *graphs[4] = {};
	Label *information_label = nullptr;
	Label *settings_label = nullptr;
	Label *percentiles_label = nullptr;

	DebugMenuSeries series_total;
	DebugMenuSeries series_cpu;
	DebugMenuSeries series_gpu;
	DebugMenuSeries series_fps;

	uint64_t last_tick_usec = 0;
	uint64_t last_graph_update_usec = 0;
	int detail_cursor = 0;
	int graph_interval_ms = 0;
	double last_total_ms = 0.0;
	double last_cpu_ms = 0.0;
	double last_gpu_ms = 0.0;
	double frames_per_second = DebugMenuUtil::GRAPH_MIN_FPS;

	int history_frames = DebugMenuUtil::DEFAULT_HISTORY_FRAMES;
	bool legacy_graph_scale = false;
	bool show_percentiles = false;
	bool gpu_unavailable_label = false;
	bool gpu_timing_available = true;
	int gpu_zero_frames = 0;
	uint64_t last_percentile_usec = 0;

	DebugMenuRecorder recorder;
	WorkerThreadPool::TaskID info_task = WorkerThreadPool::INVALID_TASK_ID;
	Vector<String> driver_info;
	bool info_ready = false;

	StringName toggle_action = SNAME("cycle_debug_menu");
	Callable size_changed_callable;

	void _build_ui();
	void _apply_style_visibility();
	void _process_frame();
	void _update_detail_slot(int p_slot);
	void _update_labels();
	void _update_graphs();
	void _fill_history();
	void _on_visibility_changed();
	void _set_history_value(Label *p_label, double p_milliseconds);
	void _start_info_task();
	void _gather_driver_info();
	void _poll_info_task();
	void _push_sample(double p_total_ms, double p_cpu_ms, double p_gpu_ms);

protected:
	void _notification(int p_what);
	void input(const Ref<InputEvent> &p_event) override;
	static void _bind_methods();

public:
	DebugMenuOverlay();
	~DebugMenuOverlay();

	void set_style(DebugMenu::Style p_style);
	DebugMenu::Style get_style() const { return style; }

	void set_menu_visible(bool p_visible);
	bool is_menu_visible() const { return is_visible(); }

	void update_settings_label();
	void reset_history();
	Dictionary get_stats();

	void set_legacy_graph_scale(bool p_enabled) { legacy_graph_scale = p_enabled; }

	void set_recording_enabled(bool p_enabled);
	bool is_recording_enabled() const { return recorder.is_enabled(); }
	String get_recording_path() const { return recorded_path; }

private:
	String recorded_path;
};
