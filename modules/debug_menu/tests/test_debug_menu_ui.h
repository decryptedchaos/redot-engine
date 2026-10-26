/**************************************************************************/
/*  test_debug_menu_ui.h                                                  */
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

#include "modules/debug_menu/debug_menu.h"
#include "modules/debug_menu/debug_menu_graph.h"
#include "modules/debug_menu/debug_menu_overlay.h"

#include "core/object/message_queue.h"
#include "core/object/object.h"
#include "scene/gui/box_container.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestDebugMenuUI {

static DebugMenuOverlay *create_overlay() {
	if (DebugMenu::get_singleton()) {
		DebugMenu::get_singleton()->uninstall();
	}

	SceneTree *tree = SceneTree::get_singleton();
	CHECK(tree != nullptr);
	if (!tree) {
		return nullptr;
	}
	Window *root = tree->get_root();
	CHECK(root != nullptr);
	if (!root) {
		return nullptr;
	}

	DebugMenuOverlay *overlay = memnew(DebugMenuOverlay);
	root->add_child(overlay);
	MessageQueue::get_singleton()->flush();
	return overlay;
}

static void destroy_overlay(DebugMenuOverlay *p_overlay) {
	if (!p_overlay) {
		return;
	}
	Node *parent = p_overlay->get_parent();
	if (parent) {
		parent->remove_child(p_overlay);
	}
	memdelete(p_overlay);
}

TEST_CASE("[SceneTree][DebugMenu][UI] Node tree and theme overrides") {
	DebugMenuOverlay *overlay = create_overlay();
	REQUIRE(overlay != nullptr);
	overlay->set_style(DebugMenu::VISIBLE_DETAILED);
	MessageQueue::get_singleton()->flush();

	CHECK(overlay->is_visible());
	CHECK_EQ(overlay->get_layer(), 128);

	Control *menu = Object::cast_to<Control>(overlay->get_node(NodePath("DebugMenu")));
	REQUIRE(menu != nullptr);
	CHECK_EQ(menu->get_custom_minimum_size(), Vector2(400, 400));
	CHECK_EQ(menu->get_size(), Vector2(400, 400));
	CHECK_EQ(menu->get_mouse_filter(), Control::MOUSE_FILTER_IGNORE);

	VBoxContainer *vbox = Object::cast_to<VBoxContainer>(overlay->get_node(NodePath("DebugMenu/VBoxContainer")));
	REQUIRE(vbox != nullptr);
	// The VBox is anchored 300 px wide; its height follows the content minimum size.
	CHECK_EQ(vbox->get_size().x, 300.0);
	CHECK_EQ(vbox->get_theme_constant("separation"), 0);

	Label *fps = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FPS")));
	REQUIRE(fps != nullptr);
	CHECK_EQ(fps->get_theme_font_size("font_size"), 18);
	CHECK_EQ(fps->get_theme_constant("outline_size"), 5);
	CHECK_EQ(fps->get_horizontal_alignment(), HORIZONTAL_ALIGNMENT_RIGHT);

	Label *frame_time = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FrameTime")));
	REQUIRE(frame_time != nullptr);
	CHECK_EQ(frame_time->get_theme_font_size("font_size"), 12);
	CHECK_EQ(frame_time->get_theme_constant("outline_size"), 3);

	Label *frame_number = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FrameNumber")));
	REQUIRE(frame_number != nullptr);
	CHECK(frame_number->is_visible());

	GridContainer *grid = Object::cast_to<GridContainer>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FrameTimeHistory")));
	REQUIRE(grid != nullptr);
	CHECK_EQ(grid->get_columns(), 5);
	CHECK_EQ(grid->get_child_count(false), 20);
	CHECK_EQ(grid->get_theme_constant("h_separation"), 0);
	CHECK_EQ(grid->get_theme_constant("v_separation"), 0);
	CHECK(grid->is_visible());

	Label *total_avg = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FrameTimeHistory/TotalAvg")));
	REQUIRE(total_avg != nullptr);
	CHECK_EQ(total_avg->get_theme_font_size("font_size"), 12);
	CHECK_EQ(total_avg->get_theme_constant("outline_size"), 3);
	CHECK_EQ(total_avg->get_custom_minimum_size(), Vector2(50, 0));
	CHECK_EQ(total_avg->get_horizontal_alignment(), HORIZONTAL_ALIGNMENT_RIGHT);

	Label *info = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/Information")));
	REQUIRE(info != nullptr);
	CHECK(info->is_visible());
	CHECK(info->get_modulate().a == doctest::Approx(0.752941));

	Label *settings = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/Settings")));
	REQUIRE(settings != nullptr);
	CHECK(settings->is_visible());

	Control *graph = Object::cast_to<Control>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FPSGraph/Graph")));
	REQUIRE(graph != nullptr);
	CHECK_EQ(graph->get_size(), Vector2(150, 25));
	CHECK_EQ(graph->get_mouse_filter(), Control::MOUSE_FILTER_IGNORE);

	Label *title = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/TotalGraph/Title")));
	REQUIRE(title != nullptr);
	CHECK_EQ(title->get_custom_minimum_size(), Vector2(0, 27));
	CHECK_EQ(title->get_vertical_alignment(), VERTICAL_ALIGNMENT_CENTER);

	const ObjectID overlay_id = overlay->get_instance_id();
	const ObjectID menu_id = menu->get_instance_id();
	destroy_overlay(overlay);
	CHECK(ObjectDB::get_instance(overlay_id) == nullptr);
	CHECK(ObjectDB::get_instance(menu_id) == nullptr);
}

TEST_CASE("[SceneTree][DebugMenu][UI] Style transitions") {
	DebugMenuOverlay *overlay = create_overlay();
	REQUIRE(overlay != nullptr);

	Label *fps = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FPS")));
	Label *frame_number = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FrameNumber")));
	GridContainer *grid = Object::cast_to<GridContainer>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FrameTimeHistory")));
	Control *graph_row = Object::cast_to<Control>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/FPSGraph")));
	Label *info = Object::cast_to<Label>(overlay->get_node(NodePath("DebugMenu/VBoxContainer/Information")));
	REQUIRE(fps != nullptr);
	REQUIRE(frame_number != nullptr);
	REQUIRE(grid != nullptr);
	REQUIRE(graph_row != nullptr);
	REQUIRE(info != nullptr);

	overlay->set_style(DebugMenu::HIDDEN);
	MessageQueue::get_singleton()->flush();
	CHECK(!overlay->is_visible());

	overlay->set_style(DebugMenu::VISIBLE_COMPACT);
	MessageQueue::get_singleton()->flush();
	CHECK(overlay->is_visible());
	CHECK(fps->is_visible());
	CHECK(!frame_number->is_visible());
	CHECK(!grid->is_visible());
	CHECK(!graph_row->is_visible());
	CHECK(!info->is_visible());

	overlay->set_style(DebugMenu::VISIBLE_DETAILED);
	MessageQueue::get_singleton()->flush();
	CHECK(frame_number->is_visible());
	CHECK(grid->is_visible());
	CHECK(graph_row->is_visible());
	CHECK(info->is_visible());

	Dictionary stats = overlay->get_stats();
	CHECK(stats.has("fps"));
	CHECK(stats.has("frametime_avg_ms"));
	CHECK(stats.has("p99_frametime_ms"));
	CHECK(stats.has("low_1pct_fps"));
	CHECK(stats.has("gpu_timing_available"));
	CHECK_EQ((int)stats["history_frames"], 150);
	CHECK((double)stats["fps"] >= 0.0);

	overlay->reset_history();
	CHECK_EQ((int)overlay->get_stats()["history_frames"], 150);

	destroy_overlay(overlay);
}

} // namespace TestDebugMenuUI
