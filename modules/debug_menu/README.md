# Debug Menu module

Native, built-in replacement for the `debug_menu` GDScript add-on. Press <kbd>F3</kbd> in a
running project to cycle `Hidden` → `Compact` → `Detailed`.

The overlay is created in C++ and attached to the root window on the first frame. No autoload,
scene, script or project file is required. The module is enabled by default with the rest of the
engine modules; disable it with `module_debug_menu_enabled=no` at build time.

## What it shows

- FPS, last frame time in milliseconds, and the active FPS cap / V-Sync mode.
- Average, best, worst and last frame times for total, CPU and GPU work over a rolling window.
- FPS, total, CPU and GPU graphs (150×25 px, same colors and layout as the add-on).
- Hardware information (CPU, OS, build type, graphics API, adapter, driver).
- Project/rendering settings (project version, rendering method, viewport and window size,
  3D scaling, antialiasing, SSR/SSAO/SSIL/SDFGI/glow/volumetric fog, 2D MSAA).

## Project settings

| Setting | Default | Description |
| --- | --- | --- |
| `debug_menu/enabled` | `true` in editor/debug builds, `false` in release templates | Attach the overlay at startup. |
| `debug_menu/start_style` | `Hidden` | Initial style: `Hidden`, `Compact` or `Detailed`. |
| `debug_menu/toggle_action` | `cycle_debug_menu` | Input action used to cycle styles. If the action does not exist it is created with F3. |
| `debug_menu/history_frames` | `150` | Frame window for averages, extremes, graphs and percentiles (30–2000). |
| `debug_menu/graph_interval_ms` | `33` | Minimum time between graph redraws (0 = redraw every frame). |
| `debug_menu/process_while_paused` | `false` | Keep updating (and allow toggling) while the game is paused. |
| `debug_menu/graph/legacy_frametime_scale` | `false` | Reproduce the add-on's original millisecond clamp for A/B comparison. |
| `debug_menu/extra_stats/percentiles` | `false` | Show an extra line with 1% and 0.1% lows. |
| `debug_menu/extra_stats/gpu_unavailable_label` | `false` | Show `n/a` instead of `0.00` when the backend reports no GPU timings. |
| `debug_menu/recording/enabled` | `false` | Write a CSV frame log. |
| `debug_menu/recording/path` | `user://debug_menu_frames.csv` | CSV destination (one row per sampled frame). |

## Script API

The engine exposes a `DebugMenu` singleton (also available as
`Engine.get_singleton("DebugMenu")` if an autoload shadows the name):

```gdscript
DebugMenu.style = DebugMenu.VISIBLE_DETAILED
DebugMenu.cycle_style()

DebugMenu.update_settings_label()   # refresh after changing graphics settings
DebugMenu.reset_history()

var stats := DebugMenu.get_stats()
print(stats.fps, " FPS, 1% low: ", stats.low_1pct_fps)

DebugMenu.set_recording_enabled(true)
print(DebugMenu.get_recording_path())
```

`get_stats()` returns `fps`, `frametime_ms`, `frametime_avg/min/max_ms`, `cpu_ms`,
`cpu_avg/min/max_ms`, `gpu_ms`, `gpu_avg/min/max_ms`, `frame`, `history_frames`,
`p99_frametime_ms`, `p999_frametime_ms`, `low_1pct_fps`, `low_0_1pct_fps` and
`gpu_timing_available`.

Custom performance monitors `DebugMenu/FPS`, `DebugMenu/FrameTime`, `DebugMenu/CPU`,
`DebugMenu/GPU` and `DebugMenu/1% Low` are registered for the editor's Monitors tab.

## Migrating from the add-on

- Remove the `DebugMenu` autoload (or the whole `addons/debug_menu` folder) first. If a node named
  `DebugMenu` already exists at the scene root, the module skips installation to avoid a duplicate
  overlay.
- `DebugMenu.style`, `DebugMenu.visible`, `DebugMenu.update_settings_label()` and the
  `cycle_debug_menu` action keep working.
- The add-on's exported `Label` node references are gone; use `get_stats()` instead.
- Set `debug_menu/enabled=true` in `project.godot` if release exports should include the overlay.

## Notes

- Updating `Label` text is the overlay's dominant cost, so visible text is refreshed one label per
  frame (each value still updates several times per second) and graphs redraw at most every 33 ms.
  This keeps detailed mode at roughly 0.3 ms of CPU per frame in a developer build and compact mode
  lower still; `debug_menu/graph_interval_ms=0` restores per-frame graph redraws.
- The module only samples while the game runs; in the editor UI, the project manager and headless
  runs no overlay is attached.
- Hardware information is gathered on a worker thread because the first driver query can be slow.
- The frametime graphs convert milliseconds to FPS before clamping. The add-on clamped
  milliseconds to FPS bounds, which flattened those graphs; set
  `debug_menu/graph/legacy_frametime_scale=true` to compare against the original behavior.
