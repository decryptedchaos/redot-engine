# Native Debug Menu module — implementation plan

Status: implemented and verified on 2026-10-24 (Windows editor dev build, unit tests, smoke run).
Target: Redot 26.3-stable (Godot 4.5.2 base).
Scope: `modules/debug_menu/` plus this `spec/` documentation. No engine files outside the module are modified.

## 0. Outcome

A built-in Redot feature: press <kbd>F3</kbd> in any running project and get the same debug menu the
GDScript add-on in `debug_menu/` shows today — same layout, fonts, outlines, colors, FPS/CPU/GPU
graphs and hardware/settings text — implemented entirely in C++ inside an engine module, with no
add-on, autoload, `.tscn`, `.gd`, `.tres`, project file change, third-party library, or edit outside
`modules/debug_menu/`.

## 1. Hard constraints

1. **100% native C++**, compiled into the engine. No GDScript, no runtime resource loading, no
   external dependency.
2. **Self-contained module.** No changes to `main/main.cpp`, `SConstruct`, `modules/SCsub`,
   `doc/classes`, or any file outside `modules/debug_menu/` and `spec/`.
3. **No scene injection.** No autoload entry written to `project.godot`, no generated scenes or
   scripts, no writes to the user's project.
4. **Visually identical** to the add-on. Primary acceptance criterion.
5. **Release gating** by project setting; unavailable by default in release templates.
6. Leave the untracked `debug_menu/` add-on folder untouched (visual reference only) and leave the
   existing uncommitted WorldScape edits alone.

### Why native nodes are still used

The overlay is a native `CanvasLayer` subclass (`DebugMenuOverlay`) created in C++ and attached to
the root window by the module itself. That is not a scene: no `.tscn`, no script, no serialized
state, no project-side installation. Its child tree is constructed from code, mirroring the
add-on's `.tscn` property for property.

Nodes are required for exact visual parity. `Label` uses `TextServer` shaping, theme fonts,
outline/shadow draw order, alignment, line spacing, oversampling and content-scale behavior. A
node-free `RenderingServer` + `Font::draw_string` renderer would have to reimplement that pipeline
and would drift; that approach is documented as Plan B in `native-debug-menu-research.md`.

## 2. Verified facts from this checkout

| Fact | Evidence |
| --- | --- |
| Engine is Redot 26.3-stable on Godot 4.5.2 | `version.py` |
| Module SCENE init runs in `Main::setup2`, before the `SceneTree` exists | `main/main.cpp:3711` |
| `SceneTree` has an internal static `singleton` / `get_singleton()` | `scene/main/scene_tree.h:236,451`; set in constructor `scene_tree.cpp:1969`; cleared in destructor `scene_tree.cpp:2196+` |
| The message queue is created before module init | `main/main.cpp:2896` |
| `Main::start` does not flush the message queue; first flushes happen inside the live tree | `main/main.cpp:4915,4928,4947`; `scene/main/scene_tree.cpp:649,706,713` |
| Project Manager sets `Engine::is_editor_hint(true)` | `main/main.cpp:4688` |
| The main loop is deleted before module SCENE uninit | `main/main.cpp:5125` vs `5160` |
| Modules auto-detected and enabled by default; generated registration files are untracked | `modules/SCsub`, `.scons_env.json`, `git ls-files` |
| `[SceneTree]` doctest support exists in this checkout | `tests/test_main.cpp:331,383` |
| `DebugMenu` is unused by existing classes/singletons/docs | repo-wide search |

## 3. Research summary

Detailed citations and quotes are in `spec/native-debug-menu-research.md`.

- Godot 4.5 module layout, `register_types`, initialization levels, `GLOBAL_DEF`, Engine
  singletons, editor-only code, doc classes and doctool.
- `SceneTree::get_singleton()` is an internal C++ accessor, not a documented scripting API.
  `call_deferred` from module/GDExtension initialization is a known workaround without a formal
  timing guarantee; early root access from a scene-level initializer is reported to crash. The
  implementation therefore defers, validates the main loop, checks the root and retries in a
  bounded way.
- Scene-tree access is main-thread only; `call_deferred` is the documented off-thread handoff.
- `viewport_set_measure_render_time` must be enabled per viewport; values are milliseconds for the
  last frame; GPU timestamps may be unavailable (0.0) on some backends.
- Slow `OS.get_video_adapter_driver_info()` calls must stay off the main thread and be cached.
- Performance monitors are editor-facing and update about once per second; percentiles need a
  larger window than 150 frames to be meaningful.
- Redot uses MIT licensing and Redot-branded headers; clang-format v20.1.0 runs via pre-commit.
- The local add-on behavior matches upstream `godot-debug-menu` master, including the
  frametime-graph clamp issue described in D-1 below.

## 4. Architecture

### 4.1 Self-install bootstrap (no engine hook)

```
Main::setup2
  └─ initialize_modules(SCENE)
       └─ DebugMenu singleton created, ClassDB registration, GLOBAL_DEF,
          DebugMenu::_deferred_install() queued (skipped if no MessageQueue yet)
Main::start
  └─ SceneTree created (SceneTree::singleton set), autoloads added
OS::run → Main::iteration
  └─ first flush inside SceneTree::physics_process()/process()
       └─ DebugMenu::_deferred_install()
            ├─ bail if debug_menu/enabled == false
            ├─ bail if Engine::is_editor_hint()        (editor UI + Project Manager)
            ├─ bail if DisplayServer is headless
            ├─ tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop())
            ├─ if no tree/root: requeue at most 2 more times, then stop
            ├─ skip if root already has a child named "DebugMenu"
            └─ DebugMenuOverlay created (native C++ tree), added to tree->get_root()
```

Notes:

- `SceneTree::get_singleton()` exists but the implementation prefers
  `OS::get_singleton()->get_main_loop()` plus `Object::cast_to<SceneTree>` so a custom `MainLoop`
  is detected and skipped rather than crashed. The test runner initializes modules before creating
  its `MessageQueue`, so `startup()` only queues the deferred install when the queue already exists.
- Message-queue flushing re-enters newly queued calls, so the retry count is deliberately tiny
  (two) to avoid spinning inside a single flush.
- `DebugMenu.install()` is public for manual installation and tests; `Engine.get_singleton("DebugMenu")`
  always works even when the overlay is disabled.
- The feature can be removed at compile time with `module_debug_menu_enabled=no`.

### 4.2 Class model

| Class | Base | Registered | Responsibility |
| --- | --- | --- | --- |
| `DebugMenu` | `Object` | yes, plus Engine singleton `"DebugMenu"` | public API, style state, install/uninstall, settings, stats dictionary, recorder, performance monitors |
| `DebugMenuOverlay` | `CanvasLayer` | yes | native UI tree, sampling, text updates, graphs, input, info refresh |
| `DebugMenuGraph` | `Control` | no | one 150×25 graph: background + polyline, samples a series |
| `DebugMenuSeries` | plain C++ | no | fixed-size ring buffer, avg/min/max/last/percentiles |
| `DebugMenuInfo` | plain C++ | no | cached hardware/project strings, async driver-info task |
| `DebugMenuRecorder` | plain C++ | no | optional CSV frame log |
| `DebugMenuUtil` (namespace) | — | — | pure helpers: formatting, gradient sampling, cap suffix, strings |

### 4.3 File layout

```
modules/debug_menu/
├── SCsub
├── config.py
├── register_types.h/.cpp
├── debug_menu.h/.cpp
├── debug_menu_overlay.h/.cpp
├── debug_menu_graph.h/.cpp
├── debug_menu_series.h/.cpp
├── debug_menu_info.h/.cpp
├── debug_menu_recorder.h/.cpp
├── debug_menu_util.h/.cpp
├── README.md
├── doc_classes/DebugMenu.xml
├── doc_classes/DebugMenuOverlay.xml
└── tests/
    ├── test_debug_menu_series.h
    ├── test_debug_menu_util.h
    └── test_debug_menu_ui.h
```

`modules/SCsub` auto-discovers `tests/*.h` into `modules_tests.gen.h`; no other build file changes.

### 4.4 Per-frame data flow

1. `NOTIFICATION_PROCESS` on the overlay (only when installed).
2. Sample once per rendered frame:
   - `total_ms = (Time::get_ticks_usec() - last_tick) * 0.001`
   - `cpu_ms = RS::viewport_get_measured_render_time_cpu(root_rid) + RS::get_frame_setup_time_cpu()`
   - `gpu_ms = RS::viewport_get_measured_render_time_gpu(root_rid)`
3. Push into `DebugMenuSeries` (incremental sum; min/max scanned over the 150-float window).
4. Compute `fps = 1000.0 / total_avg`, the derived FPS series, and gradient color.
5. If visible: refresh exactly one text label per frame (16-slot rotation: 12 history cells, frame
   number, FPS, frame time, percentiles) and `queue_redraw()` the four graphs at most every
   `debug_menu/graph_interval_ms` (default 33 ms). Updating `Label` text is the dominant cost, so it
   is spread evenly instead of re-shaping the whole panel each frame; compact mode only refreshes
   the two labels it shows.
6. Extras read the same buffers: `get_stats()`, performance monitors, CSV recorder, percentile
   cache (recomputed at most 4 Hz only when requested).
7. Sampling continues while hidden (cheap, no visuals) so `get_stats()`, monitors and recording
   stay meaningful. This changes nothing on screen.

### 4.5 Lifecycle and shutdown

- Overlay pointer held by `ObjectID` only; `DebugMenu` never stores a raw `SceneTree *` or `Node *`.
- Overlay destructor: wait for the info task (`WorkerThreadPool::wait_for_task_completion`), stop
  the recorder and disconnect the `size_changed` connection.
- `DebugMenu` uninit (SCENE): remove performance monitors, remove the Engine singleton, delete the
  singleton. At that point the `SceneTree` and overlay are already gone
  (`main/main.cpp:5125` before `5160`), so no tree access is allowed.
- The InputMap action is left in place, matching the add-on (runtime only, not saved).
- If an autoload/node named `DebugMenu` already exists at `/root`, install is skipped with a
  verbose log to avoid double overlays.

### 4.6 Threading

- Only `OS::get_video_adapter_driver_info()` runs on a `WorkerThreadPool` task. The main thread
  polls `is_task_completed()` each frame and applies the returned `Vector<String>` to the label
  there; the worker never touches scene objects.
- `viewport_set_measure_render_time` and all RenderingServer reads happen on the main thread.
- No scene objects, Labels or RenderingServer calls are touched from the worker, so the add-on's
  `Thread.set_thread_safety_checks_enabled(false)` hack disappears.

## 5. Visual parity specification

### 5.1 Node tree mapping

| `.tscn` node | C++ construction |
| --- | --- |
| `CanvasLayer` (layer 128) | `DebugMenuOverlay`, `set_layer(128)`, `set_name("DebugMenu")`, `set_visible(false)` initially |
| `DebugMenu` Control (400×400, top-right anchors, offsets −416/8/−16/408, grow begin, mouse ignore) | `Control` with anchors L/R=1, offsets −416/8/−16/408, `set_h_grow_direction(GROW_DIRECTION_BEGIN)`, `set_mouse_filter(MOUSE_FILTER_IGNORE)` |
| `VBoxContainer` (top-right, offsets −300/0/0/374, separation 0, mouse ignore) | `VBoxContainer`, anchors L/R=1, offsets as above, `add_theme_constant_override("separation", 0)`, mouse ignore |
| `FPS` Label (green, outline black 5, line_spacing 0, font 18, right) | `Label`, overrides `font_outline_color`, `outline_size=5`, `line_spacing=0`, `font_size=18`, right alignment, green modulate |
| `FrameTime`, `FrameNumber` Labels (outline 3, font 12, right) | same with `outline_size=3`, `font_size=12` |
| `FrameTimeHistory` GridContainer (5 cols, h/v sep 0, shrink-end, mouse ignore) | `GridContainer`, `set_columns(5)`, overrides `h_separation=0`, `v_separation=0`, `set_h_size_flags(SIZE_SHRINK_END)`, mouse ignore |
| `Spacer` (60×0) and header/cell Labels (50×0, outline 3, font 12, right) | invisible `Control` min size (60,0); labels with min size (50,0) and the same overrides |
| 4 graph rows (`HBoxContainer`, alignment end, mouse ignore) + `Title` (min height 27, font 12, outline 3, vertical center) | `HBoxContainer`, `set_alignment(ALIGNMENT_END)`; `Label` with min size (0,27), `set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER)` |
| `Graph` Panel (150×25, shrink vertical, bg `Color(0,0,0,0.25098)`) | `DebugMenuGraph` with min size (150,25), `SIZE_SHRINK_BEGIN`, `MOUSE_FILTER_IGNORE`; draws the same background rect then the polyline |
| `Information` Label (white 0.752941 alpha, outline 3, font 12, right) | `Label` with `Color(1,1,1,0.752941)` modulate |
| `Settings` Label (`Color(0.8,0.84,1,0.752941)`, outline 3, font 12, right) | `Label` with that modulate |
| `visibility_changed` connection | overlay's `visibility_changed` signal handler fills histories |

Nodes keep the same names (`DebugMenu`, `VBoxContainer`, `FPS`, `FrameTimeHistory`,
`FPSGraph/Graph`, …) so `/root/DebugMenu/DebugMenu/VBoxContainer/...` paths remain compatible
with any code that walked the autoload.

### 5.2 Theme and typography

Use only theme overrides present in the `.tscn`; everything else comes from the project theme
exactly as before. Keys verified in this checkout: `font_outline_color`, `outline_size`,
`line_spacing`, `font_size`, `separation`, `h_separation`, `v_separation`, `panel`.

Labels are left at their default `mouse_filter` (STOP), matching the add-on; only nodes the
`.tscn` marks IGNORE are changed.

### 5.3 Colors and gradient

Exactly four stops, sampled with `Color::lerp` (equivalent to `Gradient`'s default sRGB linear
mode, verified in `scene/resources/gradient.h`):

| Offset | Color |
| --- | --- |
| 0.0000 | `Color::from_rgba8(239, 68, 68)` red-500 |
| 0.3333 | `Color::from_rgba8(250, 204, 21)` yellow-400 |
| 0.6667 | `Color::from_rgba8(128, 226, 95)` mixed green |
| 1.0000 | `Color::from_rgba8(56, 189, 248)` light-blue-400 |

Panel background: `Color(0, 0, 0, 0.25098)`. Graph line width 1.0, no antialiasing.

Unicode (`×`, `↑`, `↓`) is built with `String::chr(0x00D7 / 0x2191 / 0x2193)` to stay independent
of the compiler execution charset (MSVC).

### 5.4 Text formats

`"Project Version: %s\n"` · `"Rendering Method: %s\n"` · `"Viewport: %d×%d, Window: %d×%d\n"` ·
`"Viewport: %d×%d\n"` · `"3D scale (%s): %d%% = %d×%d"` · `"\n3D Antialiasing: %s"` ·
`"\nSSR: %d Steps"` · `"\nSSAO: On"` · `"\nSSIL: On"` · `"\nSDFGI: %d Cascades"` ·
`"\nGlow: On"` · `"\nVolumetric Fog: On"` · `"\n2D Antialiasing: %s"` · `"%s, %d threads\n"` ·
`"%s %s (%s %s), %s %s\n"` · `"%s, %s"` · `"Frame: %d"` · `"%d FPS"` · `"%.2f mspf"` ·
`" (cap: %d FPS"` / `" + <vsync>"` / `")"` · `"Average"` `"Best"` `"Worst"` `"Last"` `"Total:"`
`"CPU:"` `"GPU:"` `"FPS: ↑"` `"Total: ↓"` `"CPU: ↓"` `"GPU: ↓"` · placeholders
`"Loading hardware information...\n\n "` and `"Loading project information..."` · release strings
`editor` / `debug` / `release` · renderer names `Forward+`, `Forward Mobile`, `Compatibility`,
`Direct3D 12`, `Metal`, `Vulkan`, `Vulkan via MoltenVK`, `OpenGL via ANGLE`, `OpenGL ES`, `WebGL`,
`OpenGL`, `Bilinear`, `FSR 1.0`, `FSR 2.2`, `V-Sync`, `Adaptive V-Sync`, `Mailbox V-Sync`.

Number formatting uses `String::num(v, 2)` / `pad_decimals(2)` to match
`str(v).pad_decimals(2)`; covered by tests.

### 5.5 Behavior parity

- Styles: `HIDDEN` → `VISIBLE_COMPACT` → `VISIBLE_DETAILED` → `HIDDEN` via
  `(style + 1) % MAX` (`MAX = 3`); F3 by default.
- Hidden: no label/graph updates, no draw. Compact shows FPS and mspf/cap. Detailed adds the
  frame counter, history grid, graphs, hardware and settings blocks.
- Exact FPS/avg/min/max/last/color formulas are mirrored, including `floor()` for FPS and the
  last-frame mspf value.
- On show, histories are filled with current values so graphs start flat instead of trailing
  garbage.
- Info: optional project version, rendering method mapping, viewport/window sizes, 3D scaling and
  antialiasing, environment effects, 2D MSAA.
- FPS cap string uses `Engine.max_fps`, low-processor mode and the current window's V-Sync mode.
- Frame counter uses `Engine.get_frames_drawn()`.
- CPU history includes frame setup time; GPU uses the viewport's measured time.
- Resize triggers `update_settings_label()`; the public method can force a refresh after graphics
  changes.

### 5.6 Deliberate deviations (documented, opt-out-able)

| ID | Deviation | Why | Default |
| --- | --- | --- | --- |
| D-1 | Frametime graphs map ms → FPS before clamping (`1000.0 / ms`, clamp to 10–160 FPS) instead of the add-on's `clampf(ms, 10, 160)` | The add-on clamps milliseconds to FPS bounds, flattening Total/CPU/GPU graphs against the bottom (60 FPS ⇒ y ≈ 23.9 of 25). The FPS graph and color logic already use FPS. | fixed; `debug_menu/graph/legacy_frametime_scale` restores the add-on math |
| D-2 | Only the slow driver-info query runs off-thread; label updates always happen on the main thread | The add-on updates Labels and RenderingServer from a `Thread` with safety checks disabled, which is unsafe. No visual difference. | fixed |
| D-3 | Low-processor FPS cap applied only when `OS.is_in_low_processor_usage_mode()` is true, with a guard for `sleep_usec == 0` | The add-on caps displayed FPS at `1000000/sleep_usec` even when low-processor mode is off (e.g. a 144 cap shown as 100). Same text in normal runs. | fixed |
| D-4 | GPU timing availability detection (backend returns 0 for N frames) available, but only replaces the value with `n/a` when `debug_menu/extra_stats/gpu_unavailable_label` is enabled | Metal/macOS-GL backends return 0.0; parity keeps showing `0.00` by default. | off |

If exact reproduction of the add-on's graph math is wanted, D-1 can be flipped to default-legacy.

## 6. Native extras

All invisible by default; none change layout unless enabled.

- `DebugMenu.get_stats()`: `fps`, `frametime_ms`, `frametime_avg/min/max_ms`, `cpu_ms`,
  `cpu_avg/min/max_ms`, `gpu_ms`, `gpu_avg/min/max_ms`, `frame`, `history_frames`,
  `p99_frametime_ms`, `p999_frametime_ms`, `low_1pct_fps`, `low_0_1pct_fps`,
  `gpu_timing_available`.
- Percentiles / 1% and 0.1% lows via nearest-rank on a scratch copy, computed at most 4 Hz;
  documented that 150 frames is too small for stable 1% lows and that
  `debug_menu/history_frames` can raise the window.
- Optional visible percentile row (`debug_menu/extra_stats/percentiles`, default off).
- CSV recorder: `debug_menu/recording/enabled`, `debug_menu/recording/path`
  (default `user://debug_menu_frames.csv`), header plus one row per sampled frame, flush every
  60 rows.
- Performance custom monitors: `DebugMenu/FPS`, `DebugMenu/FrameTime`, `DebugMenu/CPU`,
  `DebugMenu/GPU`, `DebugMenu/1% Low` in the editor debugger's Monitors tab.
- `debug_menu/history_frames` (30–2000, restart), `debug_menu/start_style`,
  `debug_menu/process_while_paused` (default off = parity),
  `debug_menu/graph/legacy_frametime_scale` (default off),
  `debug_menu/extra_stats/gpu_unavailable_label` (default off).
- Signal: `style_changed(style)`.

## 7. Settings

```
debug_menu/enabled                            bool    true in editor/debug builds, false in release
debug_menu/start_style                        enum    HIDDEN (0) | COMPACT (1) | DETAILED (2)
debug_menu/toggle_action                      String  "cycle_debug_menu"
debug_menu/history_frames                     int     150 (30..2000, restart)
debug_menu/graph_interval_ms                  int     33 (0 = redraw every frame)
debug_menu/process_while_paused               bool    false
debug_menu/graph/legacy_frametime_scale       bool    false
debug_menu/extra_stats/percentiles            bool    false
debug_menu/extra_stats/gpu_unavailable_label  bool    false
debug_menu/recording/enabled                  bool    false
debug_menu/recording/path                     String  "user://debug_menu_frames.csv"
```

`enabled` uses `#ifdef DEBUG_ENABLED` for its default (editor and debug templates `true`, release
templates `false`), so a shipped release has no overlay unless the project explicitly opts in.
`module_debug_menu_enabled=no` removes the code entirely.

## 8. Script API

`DebugMenu` (Engine singleton and class):

```
enum Style { HIDDEN, VISIBLE_COMPACT, VISIBLE_DETAILED, MAX }
property style : int
property visible : bool
func install() / uninstall() / is_installed() -> bool
func cycle_style()
func update_settings_label()
func reset_history()
func get_stats() -> Dictionary
func set_recording_enabled(bool) / is_recording_enabled() -> bool
signal style_changed(style: int)
```

Add-on migration: `DebugMenu.style`, `DebugMenu.visible` and `DebugMenu.update_settings_label()`
keep working; exported `Label` references are gone (query `get_stats()` instead); the
`cycle_debug_menu` action is preserved.

## 9. Tests and verification

### Automated

- `test_debug_menu_series.h`: ring wrap, avg/min/max/last, fill, resize, reset, nearest-rank
  percentiles and 1% / 0.1% formulas.
- `test_debug_menu_util.h`: `format_ms` against GDScript-equivalent strings, gradient samples
  equal `Gradient` at stops/midpoints, FPS-cap suffix matrix (max_fps 0/30/60/144/1000 ×
  low-processor on/off × V-Sync modes × sleep 0), style wrap, Unicode strings.
- `test_debug_menu_ui.h` (`[SceneTree]`): instantiate `DebugMenuOverlay`, add it to the root,
  flush, and assert node names/tree shape, menu/VBox/graph geometry, theme overrides, modulation,
  right alignment, style transitions, history fill on visibility change, `DebugMenu.get_stats()`
  keys, and that the overlay and its menu are freed after removal.
- Run with `bin/redot.windows.editor.dev.x86_64.console.exe --headless --test --test-case="*DebugMenu*"`.
- Static: XML well-formedness (schema validation needs `lxml`/`xmllint`, not installed here);
  clang-format v20.1.0 is not on `PATH`, so formatting was reviewed manually.

### Verification results

| Check | Result |
| --- | --- |
| `scons platform=windows target=editor dev_build=yes debug_symbols=yes optimize=none tests=yes -j16` | succeeds, no warnings from module files |
| `--headless --test --test-case="*DebugMenu*"` | 10 test cases, 157 assertions, all pass |
| Smoke project run (windowed, `start_style=2`) | `SMOKE_INSTALLED=true`, `/root/DebugMenu` present, style 2, stats dict has `fps`/`low_1pct_fps`/150 history frames |
| F3 event via `Input.parse_input_event` | style goes 2 → 0, overlay hidden |
| Captured detailed overlay frame | matches the add-on node tree, typography, colors, graph panels and right alignment |
| Shutdown / teardown | no crashes; the headless test runner reports its font-atlas RID teardown messages (engine-level, also seen when labels are used in tests), and overlay/menu ObjectIDs are confirmed freed |

Remaining manual matrix items (DPI, content-scale modes, every renderer/AA combination, export
templates, long soak) are unchanged from the plan below.

### Manual and visual

A temporary project (`project.godot`, one `Node` scene, one autoload-free script) was run with the
built console binary at 1152×648, `debug_menu/start_style=2`. It confirmed automatic installation,
the `/root/DebugMenu` node, a live stats dictionary and the F3 style cycle; the detailed frame was
captured to PNG and compared against the add-on's `.tscn`/script by inspection.

Remaining matrix (not run in this pass): editor UI and Project Manager, editor-run game, debug and
release export templates, custom `MainLoop`, window resize, 100%/200% DPI, content-scale
`canvas_items` / `viewport`, paused toggle, remapped toggle action, autoload name collision,
V-Sync modes, FPS caps 0/30/60/144/1000, low-processor mode, GPU timing 0 vs available, all 3D
setting rows and 2D MSAA.

### Performance (measured)

The first implementation refreshed every label and all four graphs on every visible frame and cost
about 0.8–1.3 ms/frame in the developer build (a trivial 320×180 scene dropped from ~1830 to
~530–780 fps). Measured frame cost after the refresh rewrite, same build, vsync off, Forward+:

| State | Frame time | Overhead vs no overlay |
| --- | --- | --- |
| No overlay | 0.52–0.55 ms | — |
| Hidden | 0.55–0.57 ms | ~0.02 ms |
| Compact | ~0.64 ms | ~0.05–0.10 ms |
| Detailed | 0.80–0.83 ms | ~0.25–0.31 ms |

These are `optimize=none` developer-build numbers; an optimized build is proportionally cheaper.
The remaining detailed cost is one label re-shape per frame plus the 33 ms graph redraw; graphs at
0 ms interval add roughly 0.3 ms per redraw. A GPU-bound 1920×1060 run (43 fps) showed no measurable
overlay or render-time-measurement cost, and GPU-bound FPS was identical with measurement on/off.

Soak with the previous per-frame implementation was not repeated after the rewrite; the smoke run
and the full test suite pass with the new update scheduler.

### Acceptance criteria

1. `scons platform=windows target=editor dev_build=yes tests=yes` builds (done); the export
   templates and `module_debug_menu_enabled=no` were not rebuilt in this pass.
2. `--headless --test` passes, including the new tests (done: 10 cases / 157 assertions).
3. No files outside `modules/debug_menu/` and `spec/` are modified by this work (verified with
   `git status`; the pre-existing WorldScape/editor edits were left untouched).
4. Visual check: the captured detailed frame matches the add-on's geometry/typography/colors;
   only D-1 (opt-out) and platform AA differ. Not cross-checked side by side pixel-for-pixel.
5. Release behavior is implemented (`DEBUG_ENABLED` default, runtime override) but a release
   export was not produced in this pass.
6. The windowed smoke run exits cleanly. The headless test harness reports two default-theme
   texture teardown messages after UI tests; overlay and menu ObjectIDs are asserted freed.

## 10. Phases

Phases 1–7 are complete for the Windows editor dev build. Phase 0 used the add-on's `.tscn`/script
as the reference plus a captured detailed-style frame; the remaining release/DPI/soak matrix is
listed in §9.

| Phase | Work | Done when |
| --- | --- | --- |
| 0 | Baseline: run add-on in scratch project, capture reference screenshots/values | reference images and notes in the research appendix |
| 1 | Module skeleton, singleton, settings, deferred install of a placeholder overlay | editor and debug template boot; overlay attaches in game, not in editor/PM/headless |
| 2 | `DebugMenuSeries` + util + unit tests | tests green |
| 3 | Full UI tree parity, style switching, F3, resize | `[SceneTree]` UI tests green; visual A/B on compact/detailed |
| 4 | Graphs, D-1 mapping, visibility-change fill | graphs match reference except intentional D-1 |
| 5 | Info/settings labels, async driver info, environment rows | full text parity on all renderer/AA combinations |
| 6 | Extras: stats dict, percentiles, CSV, monitors, GPU availability, release gating | API tests and manual checks |
| 7 | Docs, doctool XML, formatting, full matrix, cleanup | acceptance criteria 1–6 |

## 11. Risks and mitigations

1. **Deferred bootstrap is not a documented contract.** Mitigation: queue only after registration,
   cast the main loop, check the root, bounded retries, skip editor/PM/headless, expose
   `install()`, verify every launch mode. Escalation if flaky: a three-line `Main::start` hook,
   which would need approval; the module structure stays identical.
2. **Visual drift.** Mitigation: build nodes from the `.tscn` table, `[SceneTree]` rect/theme
   tests, A/B screenshots, `String::chr` for Unicode.
3. **Graph parity conflict (D-1).** Mitigation: setting to restore legacy math.
4. **GPU timings unavailable.** Mitigation: opt-in `n/a` display, stats flag, docs.
5. **Slow driver query / thread lifetime.** Mitigation: worker task with
   `wait_for_task_completion` in the destructor, cached result.
6. **Shutdown ordering** (`delete_main_loop` before module uninit). Mitigation: `ObjectID`, no tree
   pointers, idempotent cleanup.
7. **Name collision with the old autoload.** Mitigation: skip install if `/root/DebugMenu` exists;
   document migration.
8. **Release footprint/behavior.** Mitigation: build-time module flag plus runtime setting default
   off in release.

## 12. Related files

- Research notes and citations: `spec/native-debug-menu-research.md`.
- Reference add-on (untouched): `debug_menu/`.
- New module: `modules/debug_menu/`.
