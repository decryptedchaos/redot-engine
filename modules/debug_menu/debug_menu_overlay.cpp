/**************************************************************************/
/*  debug_menu_overlay.cpp                                                */
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

#include "debug_menu_overlay.h"

#include "debug_menu_graph.h"
#include "debug_menu_info.h"
#include "debug_menu_util.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/object/callable_method_pointer.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "scene/gui/box_container.h"
#include "scene/gui/control.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_server.h"

static Label *create_label(const String &p_text, int p_font_size, int p_outline_size, HorizontalAlignment p_alignment) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_horizontal_alignment(p_alignment);
	label->add_theme_color_override("font_outline_color", Color(0, 0, 0));
	label->add_theme_constant_override("outline_size", p_outline_size);
	label->add_theme_font_size_override("font_size", p_font_size);
	return label;
}

static double fps_from_ms(double p_milliseconds) {
	// A zero frame time means "faster than the graph can show" (the add-on's 1000/0 is +inf,
	// which clamps to the top of the scale).
	return p_milliseconds > 0.0 ? 1000.0 / p_milliseconds : (double)DebugMenuUtil::GRAPH_MAX_FPS;
}

DebugMenuOverlay::DebugMenuOverlay() {
	set_name("DebugMenu");
	set_layer(128);
	set_visible(false);
	set_process(true);
	set_process_input(true);

	const int configured_history = GLOBAL_GET("debug_menu/history_frames");
	history_frames = CLAMP(configured_history, 30, 2000);
	legacy_graph_scale = GLOBAL_GET("debug_menu/graph/legacy_frametime_scale");
	show_percentiles = GLOBAL_GET("debug_menu/extra_stats/percentiles");
	graph_interval_ms = CLAMP((int)GLOBAL_GET("debug_menu/graph_interval_ms"), 0, 1000);
	gpu_unavailable_label = GLOBAL_GET("debug_menu/extra_stats/gpu_unavailable_label");

	if (bool(GLOBAL_GET("debug_menu/process_while_paused"))) {
		set_process_mode(PROCESS_MODE_ALWAYS);
	}

	const String action = GLOBAL_GET("debug_menu/toggle_action");
	if (!action.is_empty()) {
		toggle_action = action;
	}

	recorded_path = GLOBAL_GET("debug_menu/recording/path");
	if (recorded_path.is_empty()) {
		recorded_path = "user://debug_menu_frames.csv";
	}
	if (bool(GLOBAL_GET("debug_menu/recording/enabled"))) {
		set_recording_enabled(true);
	}

	series_total.resize(history_frames);
	series_cpu.resize(history_frames);
	series_gpu.resize(history_frames);
	series_fps.resize(history_frames);

	last_tick_usec = Time::get_singleton()->get_ticks_usec();
	last_percentile_usec = last_tick_usec;

	connect(SNAME("visibility_changed"), callable_mp(this, &DebugMenuOverlay::_on_visibility_changed));
}

DebugMenuOverlay::~DebugMenuOverlay() {
	if (info_task != WorkerThreadPool::INVALID_TASK_ID && WorkerThreadPool::get_singleton()) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(info_task);
		info_task = WorkerThreadPool::INVALID_TASK_ID;
	}
	recorder.stop();
}

void DebugMenuOverlay::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_build_ui();
			if (Viewport *view = Node::get_viewport()) {
				// CPU/GPU frame times stay at zero until measurement is enabled for this viewport.
				RenderingServer::get_singleton()->viewport_set_measure_render_time(view->get_viewport_rid(), true);
				size_changed_callable = callable_mp(this, &DebugMenuOverlay::update_settings_label);
				if (!view->is_connected(SNAME("size_changed"), size_changed_callable)) {
					view->connect(SNAME("size_changed"), size_changed_callable);
				}
			}
			_fill_history();
			_start_info_task();
			update_settings_label();
		} break;

		case NOTIFICATION_PROCESS: {
			_process_frame();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (info_task != WorkerThreadPool::INVALID_TASK_ID && WorkerThreadPool::get_singleton()) {
				WorkerThreadPool::get_singleton()->wait_for_task_completion(info_task);
				info_task = WorkerThreadPool::INVALID_TASK_ID;
			}
			recorder.stop();
		} break;

		default:
			break;
	}
}

void DebugMenuOverlay::input(const Ref<InputEvent> &p_event) {
	if (p_event.is_null() || toggle_action.is_empty()) {
		return;
	}
	if (p_event->is_action_pressed(toggle_action)) {
		if (DebugMenu *singleton = DebugMenu::get_singleton()) {
			singleton->cycle_style();
		} else {
			const int next = ((int)style + 1) % (int)DebugMenu::MAX;
			set_style((DebugMenu::Style)next);
		}
	}
}

void DebugMenuOverlay::_build_ui() {
	if (menu_control) {
		return;
	}

	menu_control = memnew(Control);
	menu_control->set_name("DebugMenu");
	menu_control->set_custom_minimum_size(Vector2(400, 400));
	menu_control->set_anchor(SIDE_LEFT, 1.0f);
	menu_control->set_anchor(SIDE_RIGHT, 1.0f);
	menu_control->set_offset(SIDE_LEFT, -416);
	menu_control->set_offset(SIDE_TOP, 8);
	menu_control->set_offset(SIDE_RIGHT, -16);
	menu_control->set_offset(SIDE_BOTTOM, 408);
	menu_control->set_h_grow_direction(Control::GROW_DIRECTION_BEGIN);
	menu_control->set_h_size_flags(Control::SIZE_SHRINK_END);
	menu_control->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	menu_control->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(menu_control);

	vbox = memnew(VBoxContainer);
	vbox->set_name("VBoxContainer");
	vbox->set_anchor(SIDE_LEFT, 1.0f);
	vbox->set_anchor(SIDE_RIGHT, 1.0f);
	vbox->set_offset(SIDE_LEFT, -300);
	vbox->set_offset(SIDE_TOP, 0);
	vbox->set_offset(SIDE_RIGHT, 0);
	vbox->set_offset(SIDE_BOTTOM, 374);
	vbox->set_h_grow_direction(Control::GROW_DIRECTION_BEGIN);
	vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	vbox->add_theme_constant_override("separation", 0);
	menu_control->add_child(vbox);

	fps_label = create_label("60 FPS", 18, 5, HORIZONTAL_ALIGNMENT_RIGHT);
	fps_label->set_name("FPS");
	fps_label->add_theme_constant_override("line_spacing", 0);
	fps_label->set_modulate(Color(0, 1, 0));
	vbox->add_child(fps_label);

	frame_time_label = create_label("16.67 mspf", 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
	frame_time_label->set_name("FrameTime");
	frame_time_label->set_modulate(Color(0, 1, 0));
	vbox->add_child(frame_time_label);

	frame_number_label = create_label("Frame: 0", 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
	frame_number_label->set_name("FrameNumber");
	vbox->add_child(frame_number_label);

	history_grid = memnew(GridContainer);
	history_grid->set_name("FrameTimeHistory");
	history_grid->set_columns(5);
	history_grid->set_h_size_flags(Control::SIZE_SHRINK_END);
	history_grid->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	history_grid->add_theme_constant_override("h_separation", 0);
	history_grid->add_theme_constant_override("v_separation", 0);
	vbox->add_child(history_grid);

	Control *spacer = memnew(Control);
	spacer->set_name("Spacer");
	spacer->set_custom_minimum_size(Vector2(60, 0));
	spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	history_grid->add_child(spacer);

	const char *column_headers[4] = { "Average", "Best", "Worst", "Last" };
	const char *column_header_names[4] = { "AvgHeader", "MinHeader", "MaxHeader", "LastHeader" };
	for (int i = 0; i < 4; i++) {
		Label *header = create_label(column_headers[i], 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
		header->set_name(column_header_names[i]);
		header->set_custom_minimum_size(Vector2(50, 0));
		history_grid->add_child(header);
	}

	const char *row_names[3] = { "Total", "CPU", "GPU" };
	const char *column_names[4] = { "Avg", "Min", "Max", "Last" };
	for (int row = 0; row < 3; row++) {
		Label *row_header = create_label(String(row_names[row]) + ":", 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
		row_header->set_name(String(row_names[row]) + "Header");
		row_header->set_custom_minimum_size(Vector2(50, 0));
		history_grid->add_child(row_header);

		for (int column = 0; column < 4; column++) {
			Label *value = create_label("0.00", 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
			value->set_name(String(row_names[row]) + column_names[column]);
			value->set_custom_minimum_size(Vector2(50, 0));
			value->set_modulate(Color(0, 1, 0));
			history_grid->add_child(value);
			history_value_labels[row][column] = value;
		}
	}

	const String titles[4] = {
		"FPS: " + DebugMenuUtil::unicode_arrow_up(),
		"Total: " + DebugMenuUtil::unicode_arrow_down(),
		"CPU: " + DebugMenuUtil::unicode_arrow_down(),
		"GPU: " + DebugMenuUtil::unicode_arrow_down(),
	};
	const char *graph_names[4] = { "FPSGraph", "TotalGraph", "CPUGraph", "GPUGraph" };

	for (int i = 0; i < 4; i++) {
		HBoxContainer *row = memnew(HBoxContainer);
		row->set_name(graph_names[i]);
		row->set_alignment(BoxContainer::ALIGNMENT_END);
		row->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		vbox->add_child(row);

		Label *title = create_label(titles[i], 12, 3, HORIZONTAL_ALIGNMENT_LEFT);
		title->set_name("Title");
		title->set_custom_minimum_size(Vector2(0, 27));
		title->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		title->set_h_size_flags(Control::SIZE_SHRINK_END);
		row->add_child(title);

		DebugMenuGraph *graph = memnew(DebugMenuGraph);
		graph->set_name("Graph");
		row->add_child(graph);

		graph_rows[i] = row;
		graphs[i] = graph;
	}

	graphs[GRAPH_FPS]->set_series(&series_fps);
	graphs[GRAPH_FPS]->set_scale_mode(DebugMenuGraph::SCALE_FPS);

	graphs[GRAPH_TOTAL]->set_series(&series_total);
	graphs[GRAPH_TOTAL]->set_scale_mode(DebugMenuGraph::SCALE_FRAMETIME);
	graphs[GRAPH_TOTAL]->set_legacy_frametime_scale(legacy_graph_scale);

	graphs[GRAPH_CPU]->set_series(&series_cpu);
	graphs[GRAPH_CPU]->set_scale_mode(DebugMenuGraph::SCALE_FRAMETIME);
	graphs[GRAPH_CPU]->set_legacy_frametime_scale(legacy_graph_scale);

	graphs[GRAPH_GPU]->set_series(&series_gpu);
	graphs[GRAPH_GPU]->set_scale_mode(DebugMenuGraph::SCALE_FRAMETIME);
	graphs[GRAPH_GPU]->set_legacy_frametime_scale(legacy_graph_scale);

	information_label = create_label("Loading hardware information...\n\n ", 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
	information_label->set_name("Information");
	information_label->set_modulate(Color(1, 1, 1, 0.752941f));
	vbox->add_child(information_label);

	settings_label = create_label("Loading project information...", 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
	settings_label->set_name("Settings");
	settings_label->set_modulate(Color(0.8f, 0.84f, 1.0f, 0.752941f));
	vbox->add_child(settings_label);

	if (show_percentiles) {
		percentiles_label = create_label("", 12, 3, HORIZONTAL_ALIGNMENT_RIGHT);
		percentiles_label->set_name("Percentiles");
		percentiles_label->set_modulate(Color(1, 1, 1, 0.752941f));
		percentiles_label->set_visible(false);
		vbox->add_child(percentiles_label);
	}

	_apply_style_visibility();
}

void DebugMenuOverlay::_apply_style_visibility() {
	const bool detailed = style == DebugMenu::VISIBLE_DETAILED;

	if (frame_number_label) {
		frame_number_label->set_visible(detailed);
	}
	if (history_grid) {
		history_grid->set_visible(detailed);
	}
	for (int i = 0; i < 4; i++) {
		if (graph_rows[i]) {
			graph_rows[i]->set_visible(detailed);
		}
	}
	if (information_label) {
		information_label->set_visible(detailed);
	}
	if (settings_label) {
		settings_label->set_visible(detailed);
	}
	if (percentiles_label) {
		percentiles_label->set_visible(detailed && show_percentiles);
	}
}

void DebugMenuOverlay::set_style(DebugMenu::Style p_style) {
	style = (DebugMenu::Style)CLAMP((int)p_style, 0, (int)DebugMenu::VISIBLE_DETAILED);
	set_visible(style != DebugMenu::HIDDEN);
	_apply_style_visibility();

	// Refresh right away so a style switch never shows numbers from the previous style.
	last_graph_update_usec = 0;
	detail_cursor = 0;
	if (is_visible()) {
		_update_labels();
	}

	if (DebugMenu::get_singleton()) {
		DebugMenu::get_singleton()->emit_signal(SNAME("style_changed"), (int)style);
	}
}

void DebugMenuOverlay::set_menu_visible(bool p_visible) {
	set_visible(p_visible);
}

void DebugMenuOverlay::_process_frame() {
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	if (last_tick_usec == 0) {
		last_tick_usec = now;
	}
	const double frametime = (now - last_tick_usec) * 0.001;
	last_tick_usec = now;

	_poll_info_task();

	Viewport *view = Node::get_viewport();
	if (!view) {
		return;
	}

	const RID viewport_rid = view->get_viewport_rid();
	RenderingServer *rs = RenderingServer::get_singleton();
	const double cpu_ms = rs->viewport_get_measured_render_time_cpu(viewport_rid) + rs->get_frame_setup_time_cpu();
	const double gpu_ms = rs->viewport_get_measured_render_time_gpu(viewport_rid);

	_push_sample(frametime, cpu_ms, gpu_ms);

	if (!is_visible()) {
		return;
	}

	// Text is the dominant cost of the overlay: every changed Label is re-shaped and
	// relaid out. Refreshing one label per frame in detailed mode keeps each frame's cost
	// flat instead of paying for the whole panel at once, while still refreshing every
	// value several times per second.
	if (style == DebugMenu::VISIBLE_DETAILED) {
		_update_detail_slot(detail_cursor);
		detail_cursor = (detail_cursor + 1) % DETAIL_SLOT_COUNT;
	} else {
		// Compact shows only these two labels; refresh one of them every four frames and
		// leave the hidden detail labels untouched.
		if (detail_cursor == 0) {
			_update_detail_slot(13);
		} else if (detail_cursor == 4) {
			_update_detail_slot(14);
		}
		detail_cursor = (detail_cursor + 1) % 8;
	}

	if (graph_interval_ms <= 0 || now - last_graph_update_usec >= (uint64_t)graph_interval_ms * 1000) {
		last_graph_update_usec = now;
		_update_graphs();
	}
}

void DebugMenuOverlay::_push_sample(double p_total_ms, double p_cpu_ms, double p_gpu_ms) {
	last_total_ms = p_total_ms;
	last_cpu_ms = p_cpu_ms;
	last_gpu_ms = p_gpu_ms;

	series_total.push((float)p_total_ms);
	series_cpu.push((float)p_cpu_ms);
	series_gpu.push((float)p_gpu_ms);

	const double average_total = series_total.get_avg();
	frames_per_second = average_total > 0.0 ? 1000.0 / average_total : 0.0;
	series_fps.push((float)frames_per_second);

	if (p_gpu_ms <= 0.0) {
		gpu_zero_frames++;
		if (gpu_zero_frames > 60) {
			gpu_timing_available = false;
		}
	} else {
		gpu_zero_frames = 0;
		gpu_timing_available = true;
	}

	if (recorder.is_enabled()) {
		recorder.write(Engine::get_singleton()->get_frames_drawn(), Time::get_singleton()->get_ticks_usec(), p_total_ms, p_cpu_ms, p_gpu_ms, frames_per_second);
	}
}

void DebugMenuOverlay::_update_labels() {
	for (int i = 0; i < DETAIL_SLOT_COUNT; i++) {
		_update_detail_slot(i);
	}
}

void DebugMenuOverlay::_update_detail_slot(int p_slot) {
	if (!fps_label || !frame_time_label || !frame_number_label) {
		return;
	}

	if (p_slot < 12) {
		const int row = p_slot / 4;
		const int column = p_slot % 4;
		Label *label = history_value_labels[row][column];
		if (row == 2 && gpu_unavailable_label && !gpu_timing_available) {
			if (label) {
				label->set_text("n/a");
				label->set_modulate(Color(1, 1, 1, 0.752941f));
			}
		} else {
			double value = 0.0;
			if (column == 0) {
				value = row == 0 ? series_total.get_avg() : row == 1 ? series_cpu.get_avg()
																								: series_gpu.get_avg();
			} else if (column == 1) {
				value = row == 0 ? series_total.get_min() : row == 1 ? series_cpu.get_min()
																								: series_gpu.get_min();
			} else if (column == 2) {
				value = row == 0 ? series_total.get_max() : row == 1 ? series_cpu.get_max()
																								: series_gpu.get_max();
			} else {
				value = row == 0 ? last_total_ms : row == 1 ? last_cpu_ms : last_gpu_ms;
			}
			_set_history_value(label, value);
		}
		return;
	}

	if (p_slot == 12) {
		frame_number_label->set_text("Frame: " + DebugMenuUtil::uint64_to_string(Engine::get_singleton()->get_frames_drawn()));
		return;
	}

	if (p_slot == 15) {
		if (!percentiles_label || !percentiles_label->is_visible()) {
			return;
		}
		const uint64_t now = Time::get_singleton()->get_ticks_usec();
		if (now - last_percentile_usec < 250000) {
			return;
		}
		last_percentile_usec = now;
		const double p99 = series_total.get_percentile(0.99f);
		const double p999 = series_total.get_percentile(0.999f);
		const double low_1 = p99 > 0.0 ? 1000.0 / p99 : 0.0;
		const double low_01 = p999 > 0.0 ? 1000.0 / p999 : 0.0;
		percentiles_label->set_text("1% low: " + DebugMenuUtil::format_fps(low_1) + ", 0.1% low: " + DebugMenuUtil::format_fps(low_01));
		return;
	}

	if (p_slot == 13) {
		fps_label->set_text(DebugMenuUtil::format_fps(frames_per_second));
		fps_label->set_modulate(DebugMenuUtil::frame_time_color(frames_per_second));
		return;
	}

	// Slot 14: frame time and cap suffix.
	const Color frame_time_color = DebugMenuUtil::frame_time_color(frames_per_second);

	const int max_fps = Engine::get_singleton()->get_max_fps();
	const bool low_processor = OS::get_singleton()->is_in_low_processor_usage_mode();
	const int low_processor_sleep_usec = OS::get_singleton()->get_low_processor_usage_mode_sleep_usec();
	const int vsync_mode = (int)DisplayServer::get_singleton()->window_get_vsync_mode(DisplayServer::MAIN_WINDOW_ID);

	frame_time_label->set_text(DebugMenuUtil::format_ms(last_total_ms) + " mspf" +
			DebugMenuUtil::build_cap_suffix(max_fps, low_processor, low_processor_sleep_usec, vsync_mode));
	frame_time_label->set_modulate(frame_time_color);
}

void DebugMenuOverlay::_set_history_value(Label *p_label, double p_milliseconds) {
	if (!p_label) {
		return;
	}
	p_label->set_text(DebugMenuUtil::format_ms(p_milliseconds));
	p_label->set_modulate(DebugMenuUtil::frame_time_color(fps_from_ms(p_milliseconds)));
}

void DebugMenuOverlay::_update_graphs() {
	if (style != DebugMenu::VISIBLE_DETAILED || !graphs[GRAPH_FPS]) {
		return;
	}

	graphs[GRAPH_FPS]->set_line_color(DebugMenuUtil::frame_time_color(frames_per_second));
	graphs[GRAPH_TOTAL]->set_line_color(DebugMenuUtil::frame_time_color(fps_from_ms(series_total.get_avg())));
	graphs[GRAPH_CPU]->set_line_color(DebugMenuUtil::frame_time_color(fps_from_ms(series_cpu.get_avg())));
	graphs[GRAPH_GPU]->set_line_color(DebugMenuUtil::frame_time_color(fps_from_ms(series_gpu.get_avg())));

	for (int i = 0; i < 4; i++) {
		graphs[i]->queue_redraw();
	}
}

void DebugMenuOverlay::_fill_history() {
	Viewport *view = Node::get_viewport();
	if (!view) {
		return;
	}

	const double frametime_last = MAX(0.000001, (Time::get_singleton()->get_ticks_usec() - last_tick_usec) * 0.001);
	const RID viewport_rid = view->get_viewport_rid();
	RenderingServer *rs = RenderingServer::get_singleton();
	const double cpu_last = rs->viewport_get_measured_render_time_cpu(viewport_rid) + rs->get_frame_setup_time_cpu();
	const double gpu_last = rs->viewport_get_measured_render_time_gpu(viewport_rid);

	series_fps.fill((float)(frametime_last > 0.0 ? 1000.0 / frametime_last : (double)DebugMenuUtil::GRAPH_MIN_FPS));
	series_total.fill((float)frametime_last);
	series_cpu.fill((float)cpu_last);
	series_gpu.fill((float)gpu_last);

	last_total_ms = frametime_last;
	last_cpu_ms = cpu_last;
	last_gpu_ms = gpu_last;
	frames_per_second = frametime_last > 0.0 ? 1000.0 / frametime_last : 0.0;
}

void DebugMenuOverlay::_on_visibility_changed() {
	last_graph_update_usec = 0;
	detail_cursor = 0;
	if (is_visible()) {
		_fill_history();
		_update_labels();
	}
}

void DebugMenuOverlay::_start_info_task() {
	if (info_task != WorkerThreadPool::INVALID_TASK_ID || info_ready) {
		return;
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (!pool) {
		return;
	}
	info_task = pool->add_task(callable_mp(this, &DebugMenuOverlay::_gather_driver_info), false, "DebugMenu hardware information");
}

void DebugMenuOverlay::_gather_driver_info() {
	driver_info = OS::get_singleton()->get_video_adapter_driver_info();
}

void DebugMenuOverlay::_poll_info_task() {
	if (info_task == WorkerThreadPool::INVALID_TASK_ID) {
		return;
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (pool && pool->is_task_completed(info_task)) {
		pool->wait_for_task_completion(info_task);
		info_task = WorkerThreadPool::INVALID_TASK_ID;
		info_ready = true;
		if (information_label) {
			information_label->set_text(DebugMenuInfo::build_information(driver_info));
		}
	}
}

void DebugMenuOverlay::update_settings_label() {
	if (!settings_label) {
		return;
	}
	Viewport *view = Node::get_viewport();
	if (!view) {
		return;
	}
	settings_label->set_text(DebugMenuInfo::build_settings(view));
}

void DebugMenuOverlay::reset_history() {
	series_total.clear();
	series_cpu.clear();
	series_gpu.clear();
	series_fps.clear();
	_fill_history();
}

Dictionary DebugMenuOverlay::get_stats() {
	Dictionary stats;

	const double p99 = series_total.get_percentile(0.99f);
	const double p999 = series_total.get_percentile(0.999f);

	stats["fps"] = frames_per_second;
	stats["frametime_ms"] = last_total_ms;
	stats["frametime_avg_ms"] = series_total.get_avg();
	stats["frametime_min_ms"] = series_total.get_min();
	stats["frametime_max_ms"] = series_total.get_max();
	stats["cpu_ms"] = last_cpu_ms;
	stats["cpu_avg_ms"] = series_cpu.get_avg();
	stats["cpu_min_ms"] = series_cpu.get_min();
	stats["cpu_max_ms"] = series_cpu.get_max();
	stats["gpu_ms"] = last_gpu_ms;
	stats["gpu_avg_ms"] = series_gpu.get_avg();
	stats["gpu_min_ms"] = series_gpu.get_min();
	stats["gpu_max_ms"] = series_gpu.get_max();
	stats["frame"] = (int64_t)Engine::get_singleton()->get_frames_drawn();
	stats["history_frames"] = series_total.size();
	stats["p99_frametime_ms"] = p99;
	stats["p999_frametime_ms"] = p999;
	stats["low_1pct_fps"] = p99 > 0.0 ? 1000.0 / p99 : 0.0;
	stats["low_0_1pct_fps"] = p999 > 0.0 ? 1000.0 / p999 : 0.0;
	stats["gpu_timing_available"] = gpu_timing_available;

	return stats;
}

void DebugMenuOverlay::set_recording_enabled(bool p_enabled) {
	if (p_enabled) {
		recorder.start(recorded_path);
	} else {
		recorder.stop();
	}
}

void DebugMenuOverlay::_bind_methods() {
}
