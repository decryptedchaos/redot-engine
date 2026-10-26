/**************************************************************************/
/*  debug_menu_recorder.cpp                                               */
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

#include "debug_menu_recorder.h"

#include "debug_menu_util.h"

#include "core/error/error_macros.h"

bool DebugMenuRecorder::start(const String &p_path) {
	stop();

	Error err = OK;
	file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (file.is_null() || err != OK) {
		file = Ref<FileAccess>();
		ERR_PRINT(vformat("DebugMenu: could not open frame log at %s (error %d).", p_path, (int)err));
		return false;
	}

	file->store_string("frame,timestamp_usec,total_ms,cpu_ms,gpu_ms,fps\n");
	rows_since_flush = 0;
	return true;
}

void DebugMenuRecorder::stop() {
	if (file.is_valid()) {
		file->flush();
		file = Ref<FileAccess>();
	}
	rows_since_flush = 0;
}

void DebugMenuRecorder::write(uint64_t p_frame, uint64_t p_time_usec, double p_total_ms, double p_cpu_ms, double p_gpu_ms, double p_fps) {
	if (file.is_null()) {
		return;
	}

	const String line = DebugMenuUtil::uint64_to_string(p_frame) + "," +
			DebugMenuUtil::uint64_to_string(p_time_usec) + "," +
			String::num(p_total_ms, 3) + "," +
			String::num(p_cpu_ms, 3) + "," +
			String::num(p_gpu_ms, 3) + "," +
			String::num(p_fps, 3) + "\n";

	file->store_string(line);

	rows_since_flush++;
	if (rows_since_flush >= 60) {
		file->flush();
		rows_since_flush = 0;
	}
}
