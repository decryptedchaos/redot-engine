/**************************************************************************/
/*  debug_menu_info.cpp                                                   */
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

#include "debug_menu_info.h"

#include "debug_menu_util.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/variant/variant.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/environment.h"
#include "servers/rendering/rendering_server.h"

String DebugMenuInfo::renderer_name(const String &p_rendering_method) {
	if (p_rendering_method == "forward_plus") {
		return "Forward+";
	}
	if (p_rendering_method == "mobile") {
		return "Forward Mobile";
	}
	if (p_rendering_method == "gl_compatibility") {
		return "Compatibility";
	}
	return p_rendering_method;
}

String DebugMenuInfo::build_information(const Vector<String> &p_driver_info) {
	RenderingServer *rs = RenderingServer::get_singleton();
	const String vendor = rs->get_video_adapter_vendor();
	const String adapter = rs->get_video_adapter_name();

	String adapter_string;
	if (adapter.contains(vendor.trim_suffix(" Corporation"))) {
		// Avoid repeating the vendor name (NVIDIA reports "NVIDIA Corporation" with OpenGL).
		adapter_string = adapter.trim_suffix("/PCIe/SSE2");
	} else {
		adapter_string = vendor + " - " + adapter.trim_suffix("/PCIe/SSE2");
	}

	String driver_info_string = "(unknown)";
	if (p_driver_info.size() >= 2) {
		driver_info_string = p_driver_info[1];
	}

	String release_string;
	if (OS::get_singleton()->has_feature("editor")) {
		release_string = "editor";
	} else if (OS::get_singleton()->has_feature("debug")) {
		release_string = "debug";
	} else {
		release_string = "release";
	}

	ProjectSettings *settings = ProjectSettings::get_singleton();
	const String rendering_method = settings->has_setting("rendering/renderer/rendering_method")
			? String(settings->get_setting_with_override("rendering/renderer/rendering_method"))
			: String();
	const String rendering_driver = settings->has_setting("rendering/rendering_device/driver")
			? String(settings->get_setting_with_override("rendering/rendering_device/driver"))
			: String();

	String graphics_api_string = rendering_driver;
	if (rendering_method != "gl_compatibility") {
		if (rendering_driver == "d3d12") {
			graphics_api_string = "Direct3D 12";
		} else if (rendering_driver == "metal") {
			graphics_api_string = "Metal";
		} else if (rendering_driver == "vulkan") {
			if (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("ios")) {
				graphics_api_string = "Vulkan via MoltenVK";
			} else {
				graphics_api_string = "Vulkan";
			}
		}
	} else {
		if (rendering_driver == "opengl3_angle") {
			graphics_api_string = "OpenGL via ANGLE";
		} else if (OS::get_singleton()->has_feature("mobile") || rendering_driver == "opengl3_es") {
			graphics_api_string = "OpenGL ES";
		} else if (OS::get_singleton()->has_feature("web")) {
			graphics_api_string = "WebGL";
		} else if (rendering_driver == "opengl3") {
			graphics_api_string = "OpenGL";
		}
	}

	const String processor = OS::get_singleton()->get_processor_name().replace("(R)", "").replace("(TM)", "");

	return vformat("%s, %d threads\n", processor, OS::get_singleton()->get_processor_count()) +
			vformat("%s %s (%s %s), %s %s\n",
					OS::get_singleton()->get_name(),
					OS::get_singleton()->has_feature("64") ? "64-bit" : "32-bit",
					release_string,
					OS::get_singleton()->has_feature("double") ? "double" : "single",
					graphics_api_string,
					rs->get_video_adapter_api_version()) +
			vformat("%s, %s", adapter_string, driver_info_string);
}

String DebugMenuInfo::build_settings(Viewport *p_viewport) {
	ERR_FAIL_NULL_V(p_viewport, String());

	String text;

	if (ProjectSettings::get_singleton()->has_setting("application/config/version")) {
		const String version = ProjectSettings::get_singleton()->get_setting("application/config/version");
		text += vformat("Project Version: %s\n", version);
	}

	ProjectSettings *settings = ProjectSettings::get_singleton();
	const String rendering_method = settings->has_setting("rendering/renderer/rendering_method")
			? String(settings->get_setting_with_override("rendering/renderer/rendering_method"))
			: String();
	text += vformat("Rendering Method: %s\n", renderer_name(rendering_method));

	Window *window = Object::cast_to<Window>(p_viewport);
	const Size2 visible_size = p_viewport->get_visible_rect().size;
	Size2 render_size = visible_size;
	const String times = DebugMenuUtil::unicode_times();

	if (window && window->get_content_scale_mode() == Window::CONTENT_SCALE_MODE_VIEWPORT) {
		const Size2i window_size = window->get_size();
		text += vformat("Viewport: %d%s%d, Window: %d%s%d\n",
				(int)visible_size.x, times, (int)visible_size.y,
				window_size.x, times, window_size.y);
	} else {
		if (window) {
			render_size = Size2(window->get_size());
		}
		text += vformat("Viewport: %d%s%d\n", (int)render_size.x, times, (int)render_size.y);
	}

	if (p_viewport->get_camera_3d()) {
		String scaling_3d_mode_string = "(unknown)";
		switch (p_viewport->get_scaling_3d_mode()) {
			case Viewport::SCALING_3D_MODE_BILINEAR:
				scaling_3d_mode_string = "Bilinear";
				break;
			case Viewport::SCALING_3D_MODE_FSR:
				scaling_3d_mode_string = "FSR 1.0";
				break;
			case Viewport::SCALING_3D_MODE_FSR2:
				scaling_3d_mode_string = "FSR 2.2";
				break;
			case Viewport::SCALING_3D_MODE_METALFX_SPATIAL:
				scaling_3d_mode_string = "MetalFX Spatial";
				break;
			case Viewport::SCALING_3D_MODE_METALFX_TEMPORAL:
				scaling_3d_mode_string = "MetalFX Temporal";
				break;
			default:
				break;
		}

		String antialiasing_3d_string;
		if (p_viewport->get_scaling_3d_mode() == Viewport::SCALING_3D_MODE_FSR2) {
			antialiasing_3d_string += "FSR 2.2";
		}
		if (p_viewport->get_scaling_3d_mode() != Viewport::SCALING_3D_MODE_FSR2 && p_viewport->is_using_taa()) {
			if (!antialiasing_3d_string.is_empty()) {
				antialiasing_3d_string += " + ";
			}
			antialiasing_3d_string += "TAA";
		}
		if (p_viewport->get_msaa_3d() >= Viewport::MSAA_2X) {
			if (!antialiasing_3d_string.is_empty()) {
				antialiasing_3d_string += " + ";
			}
			antialiasing_3d_string += itos((int64_t)1 << (int)p_viewport->get_msaa_3d()) + times + " MSAA";
		}
		if (p_viewport->get_screen_space_aa() == Viewport::SCREEN_SPACE_AA_FXAA) {
			if (!antialiasing_3d_string.is_empty()) {
				antialiasing_3d_string += " + ";
			}
			antialiasing_3d_string += "FXAA";
		}

		const float scaling_3d_scale = p_viewport->get_scaling_3d_scale();
		text += vformat("3D scale (%s): %d%% = %d%s%d",
				scaling_3d_mode_string,
				(int)(scaling_3d_scale * 100.0f),
				(int)(render_size.x * scaling_3d_scale),
				times,
				(int)(render_size.y * scaling_3d_scale));

		if (!antialiasing_3d_string.is_empty()) {
			text += vformat("\n3D Antialiasing: %s", antialiasing_3d_string);
		}

		const Ref<World3D> world = p_viewport->get_camera_3d()->get_world_3d();
		const Ref<Environment> environment = world.is_valid() ? world->get_environment() : Ref<Environment>();
		if (environment.is_valid()) {
			if (environment->is_ssr_enabled()) {
				text += vformat("\nSSR: %d Steps", environment->get_ssr_max_steps());
			}
			if (environment->is_ssao_enabled()) {
				text += "\nSSAO: On";
			}
			if (environment->is_ssil_enabled()) {
				text += "\nSSIL: On";
			}
			if (environment->is_sdfgi_enabled()) {
				text += vformat("\nSDFGI: %d Cascades", environment->get_sdfgi_cascades());
			}
			if (environment->is_glow_enabled()) {
				text += "\nGlow: On";
			}
			if (environment->is_volumetric_fog_enabled()) {
				text += "\nVolumetric Fog: On";
			}
		}
	}

	if (p_viewport->get_msaa_2d() >= Viewport::MSAA_2X) {
		text += vformat("\n2D Antialiasing: %d%s MSAA", (int64_t)1 << (int)p_viewport->get_msaa_2d(), times);
	}

	return text;
}
