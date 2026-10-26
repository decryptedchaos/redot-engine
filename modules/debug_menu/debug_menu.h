/**************************************************************************/
/*  debug_menu.h                                                          */
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

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/binder_common.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class DebugMenuOverlay;

/// Engine singleton that owns the native debug menu overlay.
/// The overlay installs itself on the first frame; no autoload or scene file is involved.
class DebugMenu : public Object {
	GDCLASS(DebugMenu, Object);

public:
	enum Style {
		HIDDEN,
		VISIBLE_COMPACT,
		VISIBLE_DETAILED,
		MAX,
	};

private:
	static DebugMenu *singleton;

	ObjectID overlay_id;
	Style style = HIDDEN;
	int install_attempts = 0;
	bool monitors_registered = false;
	bool recording_requested = false;
	String recording_path;
	StringName toggle_action = SNAME("cycle_debug_menu");

	bool _try_install();
	void _ensure_input_action();
	void _register_monitors();
	void _unregister_monitors();

protected:
	static void _bind_methods();
	void _deferred_install();

	double _monitor_fps();
	double _monitor_frametime();
	double _monitor_cpu();
	double _monitor_gpu();
	double _monitor_low_1pct();

public:
	static void register_project_settings();
	static DebugMenu *get_singleton() { return singleton; }

	DebugMenu();
	~DebugMenu();

	void startup();
	void shutdown();

	DebugMenuOverlay *get_overlay() const;

	void install();
	void uninstall();
	bool is_installed() const;

	void set_style(Style p_style);
	Style get_style() const { return style; }
	void cycle_style();

	void set_visible(bool p_visible);
	bool is_visible() const;

	void update_settings_label();
	void reset_history();
	Dictionary get_stats();

	void set_recording_enabled(bool p_enabled);
	bool is_recording_enabled() const;
	String get_recording_path() const { return recording_path; }
};

VARIANT_ENUM_CAST(DebugMenu::Style);
