/**************************************************************************/
/*  debug_menu.cpp                                                        */
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

#include "debug_menu.h"

#include "debug_menu_overlay.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/message_queue.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "main/performance.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"

DebugMenu *DebugMenu::singleton = nullptr;

void DebugMenu::register_project_settings() {
#ifdef DEBUG_ENABLED
	const bool default_enabled = true;
#else
	const bool default_enabled = false;
#endif

	GLOBAL_DEF("debug_menu/enabled", default_enabled);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "debug_menu/start_style", PROPERTY_HINT_ENUM, "Hidden,Compact,Detailed"), 0);
	GLOBAL_DEF("debug_menu/toggle_action", String("cycle_debug_menu"));
	GLOBAL_DEF(PropertyInfo(Variant::INT, "debug_menu/history_frames", PROPERTY_HINT_RANGE, "30,2000,1"), 150);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "debug_menu/graph_interval_ms", PROPERTY_HINT_RANGE, "0,1000,1"), 33);
	GLOBAL_DEF("debug_menu/process_while_paused", false);
	GLOBAL_DEF("debug_menu/graph/legacy_frametime_scale", false);
	GLOBAL_DEF("debug_menu/extra_stats/percentiles", false);
	GLOBAL_DEF("debug_menu/extra_stats/gpu_unavailable_label", false);
	GLOBAL_DEF("debug_menu/recording/enabled", false);
	GLOBAL_DEF("debug_menu/recording/path", String("user://debug_menu_frames.csv"));
}

DebugMenu::DebugMenu() {
	singleton = this;
}

DebugMenu::~DebugMenu() {
	_unregister_monitors();
	if (singleton == this) {
		singleton = nullptr;
	}
}

void DebugMenu::startup() {
	style = (Style)CLAMP((int)GLOBAL_GET("debug_menu/start_style"), 0, (int)VISIBLE_DETAILED);

	recording_path = GLOBAL_GET("debug_menu/recording/path");
	if (recording_path.is_empty()) {
		recording_path = "user://debug_menu_frames.csv";
	}
	recording_requested = GLOBAL_GET("debug_menu/recording/enabled");

	const String action = GLOBAL_GET("debug_menu/toggle_action");
	if (!action.is_empty()) {
		toggle_action = action;
	}

	install_attempts = 0;
	// The test runner initializes modules before creating its MessageQueue; in a normal
	// startup the queue already exists and this defers the install to the first frame.
	if (MessageQueue::get_singleton()) {
		call_deferred(SNAME("_deferred_install"));
	}
}

void DebugMenu::shutdown() {
	uninstall();
	_unregister_monitors();
	if (singleton == this) {
		singleton = nullptr;
	}
}

void DebugMenu::_deferred_install() {
	_try_install();
}

bool DebugMenu::_try_install() {
	if (is_installed()) {
		return true;
	}

	if (!bool(GLOBAL_GET("debug_menu/enabled"))) {
		return false;
	}

	// Editor UI and the project manager use a SceneTree too; never show the overlay there.
	if (Engine::get_singleton()->is_editor_hint()) {
		return false;
	}

	DisplayServer *display_server = DisplayServer::get_singleton();
	if (!display_server || display_server->get_name() == "headless") {
		return false;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		// A custom MainLoop has no root. In normal startup the first queue flush happens
		// inside the live SceneTree, so this is only a safety net.
		if (install_attempts < 2) {
			install_attempts++;
			call_deferred(SNAME("_deferred_install"));
		}
		return false;
	}

	Node *root = tree->get_root();
	if (root->has_node(NodePath("DebugMenu"))) {
		WARN_PRINT_ONCE("DebugMenu: a node named 'DebugMenu' already exists at the scene root; skipping the native overlay.");
		return false;
	}

	_ensure_input_action();

	DebugMenuOverlay *overlay = memnew(DebugMenuOverlay);
	root->add_child(overlay);
	overlay->set_style(style);
	if (recording_requested) {
		overlay->set_recording_enabled(true);
	}
	overlay_id = overlay->get_instance_id();

	_register_monitors();
	install_attempts = 0;
	return true;
}

void DebugMenu::install() {
	install_attempts = 0;
	_try_install();
}

void DebugMenu::uninstall() {
	DebugMenuOverlay *overlay = get_overlay();
	overlay_id = ObjectID();
	if (overlay) {
		overlay->queue_free();
	}
	_unregister_monitors();
}

bool DebugMenu::is_installed() const {
	return get_overlay() != nullptr;
}

DebugMenuOverlay *DebugMenu::get_overlay() const {
	if (!overlay_id.is_valid()) {
		return nullptr;
	}
	return Object::cast_to<DebugMenuOverlay>(ObjectDB::get_instance(overlay_id));
}

void DebugMenu::_ensure_input_action() {
	if (toggle_action.is_empty()) {
		return;
	}

	InputMap *input_map = InputMap::get_singleton();
	if (!input_map || input_map->has_action(toggle_action)) {
		return;
	}

	input_map->add_action(toggle_action);
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_keycode(Key::F3);
	input_map->action_add_event(toggle_action, event);
}

void DebugMenu::set_style(Style p_style) {
	style = (Style)CLAMP((int)p_style, 0, (int)VISIBLE_DETAILED);

	if (DebugMenuOverlay *overlay = get_overlay()) {
		overlay->set_style(style);
	} else if (style != HIDDEN) {
		install();
	}
}

void DebugMenu::cycle_style() {
	set_style((Style)(((int)style + 1) % (int)MAX));
}

void DebugMenu::set_visible(bool p_visible) {
	if (p_visible && !is_installed()) {
		install();
	}
	if (DebugMenuOverlay *overlay = get_overlay()) {
		overlay->set_menu_visible(p_visible);
	}
}

bool DebugMenu::is_visible() const {
	DebugMenuOverlay *overlay = get_overlay();
	return overlay ? overlay->is_menu_visible() : false;
}

void DebugMenu::update_settings_label() {
	if (DebugMenuOverlay *overlay = get_overlay()) {
		overlay->update_settings_label();
	}
}

void DebugMenu::reset_history() {
	if (DebugMenuOverlay *overlay = get_overlay()) {
		overlay->reset_history();
	}
}

Dictionary DebugMenu::get_stats() {
	DebugMenuOverlay *overlay = get_overlay();
	if (overlay) {
		return overlay->get_stats();
	}

	Dictionary stats;
	stats["fps"] = 0.0;
	stats["frametime_ms"] = 0.0;
	stats["frametime_avg_ms"] = 0.0;
	stats["frametime_min_ms"] = 0.0;
	stats["frametime_max_ms"] = 0.0;
	stats["cpu_ms"] = 0.0;
	stats["cpu_avg_ms"] = 0.0;
	stats["cpu_min_ms"] = 0.0;
	stats["cpu_max_ms"] = 0.0;
	stats["gpu_ms"] = 0.0;
	stats["gpu_avg_ms"] = 0.0;
	stats["gpu_min_ms"] = 0.0;
	stats["gpu_max_ms"] = 0.0;
	stats["frame"] = (int64_t)Engine::get_singleton()->get_frames_drawn();
	stats["history_frames"] = 0;
	stats["p99_frametime_ms"] = 0.0;
	stats["p999_frametime_ms"] = 0.0;
	stats["low_1pct_fps"] = 0.0;
	stats["low_0_1pct_fps"] = 0.0;
	stats["gpu_timing_available"] = false;
	return stats;
}

void DebugMenu::set_recording_enabled(bool p_enabled) {
	recording_requested = p_enabled;
	if (DebugMenuOverlay *overlay = get_overlay()) {
		overlay->set_recording_enabled(p_enabled);
	}
}

bool DebugMenu::is_recording_enabled() const {
	DebugMenuOverlay *overlay = get_overlay();
	return overlay ? overlay->is_recording_enabled() : recording_requested;
}

void DebugMenu::_register_monitors() {
	if (monitors_registered) {
		return;
	}

	Performance *performance = Performance::get_singleton();
	if (!performance) {
		return;
	}

	performance->add_custom_monitor("DebugMenu/FPS", callable_mp(this, &DebugMenu::_monitor_fps), Vector<Variant>());
	performance->add_custom_monitor("DebugMenu/FrameTime", callable_mp(this, &DebugMenu::_monitor_frametime), Vector<Variant>());
	performance->add_custom_monitor("DebugMenu/CPU", callable_mp(this, &DebugMenu::_monitor_cpu), Vector<Variant>());
	performance->add_custom_monitor("DebugMenu/GPU", callable_mp(this, &DebugMenu::_monitor_gpu), Vector<Variant>());
	performance->add_custom_monitor("DebugMenu/1% Low", callable_mp(this, &DebugMenu::_monitor_low_1pct), Vector<Variant>());

	monitors_registered = true;
}

void DebugMenu::_unregister_monitors() {
	if (!monitors_registered) {
		return;
	}

	Performance *performance = Performance::get_singleton();
	if (performance) {
		performance->remove_custom_monitor("DebugMenu/FPS");
		performance->remove_custom_monitor("DebugMenu/FrameTime");
		performance->remove_custom_monitor("DebugMenu/CPU");
		performance->remove_custom_monitor("DebugMenu/GPU");
		performance->remove_custom_monitor("DebugMenu/1% Low");
	}

	monitors_registered = false;
}

double DebugMenu::_monitor_fps() {
	DebugMenuOverlay *overlay = get_overlay();
	if (!overlay) {
		return 0.0;
	}
	return overlay->get_stats()["fps"];
}

double DebugMenu::_monitor_frametime() {
	DebugMenuOverlay *overlay = get_overlay();
	if (!overlay) {
		return 0.0;
	}
	return overlay->get_stats()["frametime_ms"];
}

double DebugMenu::_monitor_cpu() {
	DebugMenuOverlay *overlay = get_overlay();
	if (!overlay) {
		return 0.0;
	}
	return overlay->get_stats()["cpu_ms"];
}

double DebugMenu::_monitor_gpu() {
	DebugMenuOverlay *overlay = get_overlay();
	if (!overlay) {
		return 0.0;
	}
	return overlay->get_stats()["gpu_ms"];
}

double DebugMenu::_monitor_low_1pct() {
	DebugMenuOverlay *overlay = get_overlay();
	if (!overlay) {
		return 0.0;
	}
	return overlay->get_stats()["low_1pct_fps"];
}

void DebugMenu::_bind_methods() {
	ClassDB::bind_method(D_METHOD("install"), &DebugMenu::install);
	ClassDB::bind_method(D_METHOD("uninstall"), &DebugMenu::uninstall);
	ClassDB::bind_method(D_METHOD("is_installed"), &DebugMenu::is_installed);
	ClassDB::bind_method(D_METHOD("get_overlay"), &DebugMenu::get_overlay);

	ClassDB::bind_method(D_METHOD("set_style", "style"), &DebugMenu::set_style);
	ClassDB::bind_method(D_METHOD("get_style"), &DebugMenu::get_style);
	ClassDB::bind_method(D_METHOD("cycle_style"), &DebugMenu::cycle_style);

	ClassDB::bind_method(D_METHOD("set_visible", "visible"), &DebugMenu::set_visible);
	ClassDB::bind_method(D_METHOD("is_visible"), &DebugMenu::is_visible);

	ClassDB::bind_method(D_METHOD("update_settings_label"), &DebugMenu::update_settings_label);
	ClassDB::bind_method(D_METHOD("reset_history"), &DebugMenu::reset_history);
	ClassDB::bind_method(D_METHOD("get_stats"), &DebugMenu::get_stats);

	ClassDB::bind_method(D_METHOD("set_recording_enabled", "enabled"), &DebugMenu::set_recording_enabled);
	ClassDB::bind_method(D_METHOD("is_recording_enabled"), &DebugMenu::is_recording_enabled);
	ClassDB::bind_method(D_METHOD("get_recording_path"), &DebugMenu::get_recording_path);

	ClassDB::bind_method(D_METHOD("_deferred_install"), &DebugMenu::_deferred_install);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "style", PROPERTY_HINT_ENUM, "Hidden,Compact,Detailed"), "set_style", "get_style");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible"), "set_visible", "is_visible");

	ADD_SIGNAL(MethodInfo("style_changed", PropertyInfo(Variant::INT, "style")));

	BIND_ENUM_CONSTANT(HIDDEN);
	BIND_ENUM_CONSTANT(VISIBLE_COMPACT);
	BIND_ENUM_CONSTANT(VISIBLE_DETAILED);
	BIND_ENUM_CONSTANT(MAX);
}
