# hymission

`hymission` is a fork of [gfhdhytghd/hymission](https://github.com/gfhdhytghd/hymission) with one unified workspace-lane overview. It projects Hyprland's live scrolling geometry directly on scrolling layouts and adapts native layout targets on other layouts.

> [!IMPORTANT]
> This is not a from-scratch plugin. The original Mission Control overview, core plugin architecture, and layout solver come from upstream Hymission. This fork unifies the overview around workspace lanes, preserving native geometry on non-scrolling layouts and using direct layout projection on scrolling layouts.

> [!WARNING]
> Hyprland plugins run inside the compositor process. Install plugins only from sources you trust.
> `hymission` may not work correctly on NVIDIA GPUs/drivers.

> [!WARNING]
> The fork was developed largely with OpenAI Codex assistance and has been manually audited. The Scrolling Overview that I worked on was largely manually developed and audited by me. Keep that development context in mind when evaluating it.

## Work added in this fork

- Implemented one overview for all Hyprland layout algorithms, using native layout targets and direct scrolling-layout projection where available.
- Added animated overview entry and exit, column-aware focus and navigation, configurable center/fit behavior, dynamic relayout, and animated workspace-to-workspace transitions.
- Built a unified scene with visible neighboring workspace lanes, optional empty workspaces, configurable gaps and scaling, and per-workspace wallpaper viewport zoom.
- Added cross-layout window dragging: scrolling lanes provide insertion targets and edge scrolling, native lanes swap with the nearest tiled target, and floating drops map into the lane. Cancellation or an invalid drop leaves layout geometry unchanged.
- Added external Wayland drag-and-drop support so files can activate overview windows or workspaces, including scrolling-lane and workspace-edge navigation.
- Integrated the unified overview with Hymission's configuration, gesture, dispatcher, rendering, and state-management systems.

## Upstream Hymission features retained

- Mission Control-style overview with animated window previews
- Monitor and special-workspace scope control with default config scope, `onlycurrentworkspace`, and `forceall`
- Mouse, keyboard, and trackpad-driven overview interaction
- Gesture-only `recommand` mode for two-sided `toggle` gestures
- Unified workspace lanes across participating workspaces
- Monitor-aware workspace lanes
- Pinned-window and special-workspace-aware behavior

## Scrolling Overview Demo

https://github.com/user-attachments/assets/e23e9da5-a90e-4ad0-9e51-5e19419e1332


## Earlier Non-scrolling Overview Demo

https://github.com/user-attachments/assets/d3e7625f-a831-474a-ac85-02dca635beda


## Installation

### Install with `hyprpm`

`hyprpm` is the preferred user-facing install path in the Hyprland ecosystem.

```sh
hyprpm update
hyprpm add https://github.com/dxledev/hymission
hyprpm enable hymission
hyprpm reload
```

If you use Hyprland's permission system, you may need to allow `hyprpm` in your config:

```conf
permission = /usr/(bin|local/bin)/hyprpm, plugin, allow
```

Do not also manually `hyprctl plugin load` the same plugin if you manage it through `hyprpm`.

### Manual build and reload

For local development, `hymission` uses CMake and outputs `build-cmake/libhymission.so`.

Requirements:

- Hyprland 0.55.2 or 0.56.x development headers for the exact Hyprland build you are running
- `cmake`
- `pkg-config`
- a C++23-capable compiler

The same source tree supports Hyprland 0.55.2 and 0.56.x. Hyprland plugins are tied to the compositor's build, so rebuild `hymission` after upgrading Hyprland instead of reusing the old `.so`.

Build:

```sh
cmake -DCMAKE_BUILD_TYPE=Release -B build-cmake
cmake --build build-cmake -j"$(nproc)"
ctest --test-dir build-cmake --output-on-failure
```

Safe reload sequence on this machine:

```sh
hyprctl plugin unload "$(pwd)/build/libhymission.so"
hyprctl plugin unload "$(pwd)/build-cmake/libhymission.so"
hyprctl plugin unload "$(pwd)/build-meson/libhymission.so"
hyprctl plugin load "$(pwd)/build-cmake/libhymission.so"
hyprctl plugin list
```

`plugin not loaded` is expected when the unloaded path is not the active copy.

Build outputs:

- Plugin: `build-cmake/libhymission.so`
- Layout demo: `build-cmake/hymission-layout-demo`
- Layout test: `build-cmake/hymission-mission-layout-test`
- Logic test: `build-cmake/hymission-overview-logic-test`

## Usage

### Dispatchers

```conf
bind = SUPER, TAB, hymission:toggle
bind = SUPER SHIFT, TAB, hymission:open
bind = SUPER CTRL, TAB, hymission:close
bind = SUPER, C, hymission:toggle,onlycurrentworkspace
bind = SUPER, A, hymission:toggle,forceall
bind = SUPER, M, hymission:debug_current_layout
```

| Dispatcher | Description |
| --- | --- |
| `hymission:toggle` | Toggle overview. Supports `onlycurrentworkspace` and `forceall`. |
| `hymission:open` | Open overview. Supports `onlycurrentworkspace` and `forceall`. |
| `hymission:close` | Close overview. |
| `hymission:debug_current_layout` | Compute the current layout and show a notification summary without entering overview. |

Scope arguments:

- no argument: use `show_special` to decide whether currently visible special workspaces are included
- `onlycurrentworkspace`: exclude special workspaces
- `forceall`: include currently visible special workspaces

All arguments open the same owner-monitor workspace-lane overview. They do not select a different overview or expand it across monitors.

### Toggle Switch Mode

`toggle_switch_mode` only affects `hymission:toggle`.

With a binding such as `bind = SUPER, TAB, hymission:toggle` and:

```conf
toggle_switch_mode = 1
switch_toggle_auto_next = 1
switch_release_key = Super_L
```

- the first `SUPER+TAB` opens overview as a switch session
- repeated `TAB` presses while `SUPER` stays held cycle to the next overview target
- releasing `SUPER` commits the current selection and exits overview

`hymission:open`, `hymission:close`, and gesture paths keep their normal behavior. Toggle switch mode is meant for modifier-backed `hymission:toggle` bindings such as `ALT+TAB` / `SUPER+TAB`.

### Lua dispatchers

When using Hyprland's Lua config, Hymission exposes native plugin functions under `hl.plugin.hymission`:

```lua
hl.bind("SUPER + TAB", hl.plugin.hymission.toggle())
hl.bind("SUPER + A", function()
    hl.dispatch(hl.plugin.hymission.toggle("forceall"))
end)
hl.bind("SUPER + S", hl.plugin.hymission.open("onlycurrentworkspace"))
hl.bind("SUPER + Escape", hl.plugin.hymission.close())
hl.bind("SUPER + 1", hl.plugin.hymission.workspace("1"))
```

Available functions:

- `hl.plugin.hymission.toggle(args?)` returns an `HL.Dispatcher`
- `hl.plugin.hymission.open(args?)` returns an `HL.Dispatcher`
- `hl.plugin.hymission.close()` returns an `HL.Dispatcher`
- `hl.plugin.hymission.debug_current_layout()` returns an `HL.Dispatcher`
- `hl.plugin.hymission.workspace(args)` returns an `HL.Dispatcher` that routes through Hymission's workspace-transition interception while overview is visible
- `hl.plugin.hymission.dispatch(name, args?)` returns an `HL.Dispatcher`
- `hl.plugin.hymission.gesture(table|string, disable_inhibit?)`

`toggle` and `open` accept the same optional scope arguments as the legacy dispatchers: `forceall` and `onlycurrentworkspace`.

### Gestures

Use Hyprland's official gesture syntax. Scrolling layout panning can use either Hymission's compatibility gesture or Hyprland's native `scrollMove`:

```conf
gesture = 4, vertical, dispatcher, hymission:toggle,forceall
gesture = 4, vertical, dispatcher, hymission:toggle,recommand
gesture = 4, vertical, dispatcher, hymission:open,onlycurrentworkspace
gesture = 3, horizontal, dispatcher, hymission:scroll,layout
# or: gesture = 3, horizontal, scrollMove
gesture = 3, vertical, workspace
```

Lua config should register Hymission gestures through `hl.plugin.hymission.gesture(...)` instead of `hl.gesture({ action = function() ... end })` when you want continuous overview progress:

```lua
hl.plugin.hymission.gesture({
    fingers = 4,
    direction = "vertical",
    action = "toggle",
    args = "forceall",
})

hl.plugin.hymission.gesture({
    fingers = 4,
    direction = "vertical",
    action = "toggle",
    recommand = true,
})

hl.plugin.hymission.gesture({
    fingers = 4,
    direction = "vertical",
    action = "open",
    scope = "onlycurrentworkspace",
})

hl.plugin.hymission.gesture({
    fingers = 3,
    direction = "horizontal",
    action = "scroll",
    mode = "layout",
})

-- Native alternative:
-- hl.gesture({ fingers = 3, direction = "horizontal", action = "scroll_move" })

hl.plugin.hymission.gesture({
    fingers = 3,
    direction = "vertical",
    action = "workspace",
})
```

Optional gesture fields are `mods`, `scale`, and `disable_inhibit`.

Gesture notes:

- `vertical` and `horizontal` are supported for plugin-managed overview gestures; `hymission:scroll,layout` also supports `swipe`
- unofficial shorthand such as `gesture = ..., hymission:toggle,...` is not supported
- default gesture semantics are state-aware: hidden overview opens in the configured direction, and visible `hymission:toggle,*` overview can close in either swipe direction
- `recommand` is gesture-only and is only valid with `hymission:toggle`
- scrolling layout movement supports both `hymission:scroll,layout` and Hyprland's official `scrollMove` / Lua `scroll_move`
- workspace swipes should use Hyprland's standard `gesture = ..., workspace`; Hymission already intercepts that path while overview is visible
- in `recommand` mode, one side uses `forceall` scope and the other uses `onlycurrentworkspace` scope
- switching from one visible `recommand` side to the other only works in the side-changing direction; it must pass through hidden state and then cross a small transfer gap before the opposite side starts opening
- swiping the other visible `recommand` direction only exits overview back to hidden and does not continue into the opposite side
- a gesture that started from hidden can still be pulled back to cancel, but it cannot become a new visible-start close/transfer gesture until you lift and swipe again
- release still uses a `50% + velocity` commit rule

## Configuration

All user-facing settings live under `plugin:hymission`.

Example:

```conf
plugin {
    hymission {
        outer_padding_top = 92
        outer_padding_right = 32
        outer_padding_bottom = 32
        outer_padding_left = 32
        row_spacing = 32
        column_spacing = 32
        min_window_length = 120
        min_preview_short_edge = 32
        small_window_boost = 1.35
        max_preview_scale = 0.95
        workspace_overview_max_preview_scale = 0.95
        min_slot_scale = 0.10
        natural_scale_flex = 0.22
        layout_engine = grid
        layout_scale_weight = 1.0
        layout_space_weight = 0.10

        expand_selected_window = 1
        multi_workspace_expand_selected_window = 1
        overview_focus_follows_mouse = 1
        refresh_previews_on_config_reload = 1
        strip_theme_surface_feedback_frames = 300
        multi_workspace_sort_recent_first = 1
        niri_mode = 1
        niri_scroll_pixels_per_delta = 1.0
        niri_layout_scale = 1.0
        niri_overview_scale = 0.65
        niri_window_gaps = -1.0
        niri_workspace_gap = -1.0
        niri_multi_ws_scale = 0.18
        niri_workspace_scale = 1.0
        niri_strip_workspace_zoom = 2.0
        niri_mode_show_empty_workspaces_btwn = 1
        niri_mode_wallpaper_zoom = 0
        niri_mode_wallpaper_zoom_background_color = "#0D0F14FF"
        niri_mode_wallpaper_zoom_layer_namespaces = "awww-daemon"
        niri_mode_wallpaper_zoom_layer_refresh_ms = 100
        niri_preview_disabled = 0
        niri_overview_animations = 1
        niri_overview_open_close_speed_multiplier = 1.5
        niri_dnd_edge_view_scroll_trigger = 30.0
        niri_dnd_edge_view_scroll_delay_ms = 100
        niri_dnd_edge_view_scroll_max_speed = 1500.0
        niri_dnd_edge_workspace_switch_trigger = 50.0
        niri_dnd_edge_workspace_switch_delay_ms = 100
        niri_dnd_edge_workspace_switch_max_speed = 1500.0
        niri_dnd_hold_to_activate_delay_ms = 750
        toggle_switch_mode = 1
        switch_toggle_auto_next = 1
        switch_release_key = Super_L
        gesture_invert_vertical = 0
        one_workspace_per_row = 0
        only_active_workspace = 1
        only_active_monitor = 0
        show_special = 0
        workspace_change_keeps_overview = 1
        damage_tracking_override = 1
        close_special_workspaces_on_open = 1

        workspace_strip_anchor = left
        workspace_strip_empty_mode = existing
        workspace_strip_thickness = 160
        workspace_strip_gap = 24
        hide_bar_when_strip = 1
        hide_namespaces_overview = hypr-dock,waybar,chromack,wardnc,wardbnc,dashboard,rofi
        hide_layers_when_overview = 1
        hide_namespace_overview_niri_scrolling = chromack,wardnc,wardbnc,dashboard,rofi
        hide_bar_animation = 1
        hide_bar_animation_blur = 1
        hide_bar_animation_move_multiplier = 0.8
        hide_bar_animation_scale_divisor = 1.1
        hide_bar_animation_alpha_end = 0
        bar_single_mission_control = 0
        show_focus_indicator = 0

        debug_logs = 0
        debug_surface_logs = 0
    }
}
```

Lua config uses the same names under `plugin.hymission`:

```lua
hl.config({
    plugin = {
        hymission = {
            outer_padding_top = 92,
            layout_engine = "grid",
            niri_mode = 1,
            niri_layout_scale = 1.0,
            niri_overview_scale = 0.65,
            niri_workspace_gap = -1.0,
            niri_multi_ws_scale = 0.18,
            niri_mode_wallpaper_zoom = 0,
            niri_mode_wallpaper_zoom_background_color = "#0D0F14FF",
            niri_mode_wallpaper_zoom_layer_namespaces = "awww-daemon",
            niri_mode_wallpaper_zoom_layer_refresh_ms = 100,
            niri_preview_disabled = 0,
            niri_overview_animations = 1,
            niri_overview_open_close_speed_multiplier = 1.5,
            niri_dnd_edge_view_scroll_trigger = 30.0,
            niri_dnd_edge_view_scroll_delay_ms = 100,
            niri_dnd_edge_view_scroll_max_speed = 1500.0,
            niri_dnd_edge_workspace_switch_trigger = 50.0,
            niri_dnd_edge_workspace_switch_delay_ms = 100,
            niri_dnd_edge_workspace_switch_max_speed = 1500.0,
            niri_dnd_hold_to_activate_delay_ms = 750,
            multi_workspace_expand_selected_window = 1,
            switch_release_key = "Super_L",
            workspace_strip_anchor = "left",
        },
    },
})
```

### Layout options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `outer_padding` | int | `32` | Legacy fallback for all four edge paddings. |
| `outer_padding_top` | int | `92` | Top padding for the overview content area. |
| `outer_padding_right` | int | `32` | Right padding for the overview content area. |
| `outer_padding_bottom` | int | `32` | Bottom padding for the overview content area. |
| `outer_padding_left` | int | `32` | Left padding for the overview content area. |
| `row_spacing` | int | `32` | Vertical spacing between preview rows. |
| `column_spacing` | int | `32` | Horizontal spacing between preview columns. |
| `min_window_length` | int | `120` | Legacy layout-solver setting; it does not affect unified workspace lanes. |
| `min_preview_short_edge` | int | `32` | Legacy layout-solver setting; it does not affect unified workspace lanes. |
| `small_window_boost` | float | `1.35` | Legacy layout-solver setting; it does not affect unified workspace lanes. |
| `max_preview_scale` | float | `0.95` | Maximum preview scale used by scrolling-layout fit calculations. |
| `workspace_overview_max_preview_scale` | float | `0.95` | Active-workspace override for the scrolling-layout preview-fit cap. |
| `min_slot_scale` | float | `0.10` | Minimum scale for workspace-lane viewports, including native-layout lanes. |
| `natural_scale_flex` | float | `0.22` | Legacy layout-solver setting; it does not affect unified workspace lanes. |
| `layout_engine` | string | `grid` | Legacy compatibility setting. `grid`, `natural`, `apple`, `expose`, and `mission-control` do not select or change the unified overview projection. |
| `layout_scale_weight` | float | `1.0` | Legacy layout-solver setting; it does not affect unified workspace lanes. |
| `layout_space_weight` | float | `0.10` | Legacy layout-solver setting; it does not affect unified workspace lanes. |
| `one_workspace_per_row` | bool | `0` | Legacy layout-solver setting; it does not affect workspace-lane placement. |

### Behavior options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `expand_selected_window` | bool | `1` | Legacy compatibility setting; selected-window expansion does not change workspace-lane geometry. |
| `multi_workspace_expand_selected_window` | bool | `1` | Legacy compatibility setting; selected-window expansion does not change workspace-lane geometry. |
| `overview_focus_follows_mouse` | bool | `1` | Keep the overview selection aligned with hover, and sync real focus when allowed. Hover retargeting is frame-coalesced for smoother animation, and the scene stays visually anchored when real focus crosses workspaces. |
| `overview_center_cursor_on_hover_focus` | bool | `1` | Move the cursor to the newly focused preview center after scrolling overview recenters. This prevents a stationary edge hover from chaining through adjacent windows. |
| `refresh_previews_on_config_reload` | bool | `1` | Repaint cached workspace-strip preview snapshots after Hyprland config reloads, so live theme changes update preview colors while overview is open. |
| `strip_theme_surface_feedback_frames` | int | `300` | Number of frames to force surface feedback (sending wl_surface.frame callbacks to background windows) after a theme or config reload, giving hidden applications time to receive the reload and repaint their colors. |
| `multi_workspace_sort_recent_first` | bool | `1` | Legacy compatibility setting; native-layout positions remain controlled by Hyprland. |
| `niri_mode` | bool | `1` | Accepted compatibility key. The unified overview is always active; setting this to `0` does not restore the previous overview mode. |
| `niri_scroll_pixels_per_delta` | float | `1.0` | Multiplier for `hymission:scroll,layout` movement outside overview. A value of `1.0` maps roughly one `gestures:workspace_swipe_distance` of finger travel to one viewport of scrolling-layout movement. Native `scrollMove` ignores this option. |
| `niri_layout_scale` | float | `1.0` | Extra scale applied to workspace lane contents across layout algorithms. Values are clamped to `0.50` - `2.0`. |
| `niri_overview_scale` | float | `0.65` | Extra zoom factor for scrolling-layout windows in the unified overview. Lower values reveal more neighboring windows in the scroll order; values are clamped to `0.05` - `1.0`. |
| `niri_window_gaps` | float | `-1.0` | Visible gap between direct scrolling-layout previews. `-1.0` uses the current `general:gaps_in`; non-negative values are clamped to `0.0` - `160.0`. Legacy `niri_single_ws_gap_pixels` and `niri_single_ws_gap_multiplier` remain fallback inputs when explicitly set. |
| `niri_workspace_gap` | float | `-1.0` | Visible gap between workspace lanes. Values below `0.0` fall back to `general:gaps_out`; `niri_multi_ws_gap` remains supported as a legacy fallback. |
| `niri_multi_ws_scale` | float | `0.18` | Scale of each workspace lane in the shared overview scene. Values are clamped to `0.05` - `0.24`. |
| `niri_workspace_scale` | float | `1.0` | Legacy workspace-strip thumbnail scale key. It does not select a different overview mode. |
| `niri_strip_workspace_zoom` | float | `2.0` | Legacy workspace-strip zoom key. It does not select a different overview mode. |
| `niri_mode_show_empty_workspaces_btwn` | bool | `1` | Include numeric empty-workspace gaps plus one empty leaf workspace on each occupied edge. Set to `0` to only show existing workspace objects. |
| `niri_mode_wallpaper_zoom` | bool | `0` | Capture the full monitor background, reserved work-area space, and configured layer surfaces when overview opens and render them without blur inside each animated workspace viewport, including empty workspaces. When disabled, workspace viewports use the blurred placeholder card. |
| `niri_mode_wallpaper_zoom_background_color` | string | `#0D0F14FF` | Background behind the scaled workspace wallpapers when `niri_mode_wallpaper_zoom = 1`. Accepts Hyprland color syntax, including `#RRGGBBAA`; the alpha channel controls whether the original desktop remains visible behind the virtual desktops. |
| `niri_mode_wallpaper_zoom_layer_namespaces` | string | `awww-daemon` | Comma-separated ECMAScript regular expressions matched against complete layer namespaces for capture, hiding, and workspace zoom. Background and bottom layers zoom behind windows; matched top and overlay layers zoom in front. Plain names remain exact matches; use `.*` for a wildcard. |
| `niri_mode_wallpaper_zoom_layer_refresh_ms` | int | `100` | Refresh interval for retained top and overlay layer snapshots while overview is open. Set to `0` to disable live refresh. Values below `16` are clamped to `16`. |
| `niri_preview_disabled` | bool | `0` | Legacy compatibility option. The unified overview has no separate workspace preview strip. |
| `niri_overview_animations` | bool | `1` | Keep live Hyprland window movement available while the unified overview is open. Open/close and relayout motion use `windowsMove`; workspace switching uses `workspaces`. |
| `niri_overview_open_close_speed_multiplier` | float | `1.5` | Speed multiplier applied to the live `windowsMove` animation for overview open and close transitions. The configured curve and style are preserved. |
| `niri_drag_preview_alpha` | float | `0.75` | Opacity of a window while it is being dragged in the unified overview. |
| `niri_drag_edge_scroll_trigger` | float | `30.0` | Width in logical pixels of the scrolling-layout edge zone used while dragging a window. |
| `niri_drag_edge_scroll_delay_ms` | int | `100` | Delay before drag edge scrolling begins. |
| `niri_drag_edge_scroll_max_speed` | float | `1500.0` | Maximum drag edge-scroll speed in logical pixels per second. |
| `niri_dnd_edge_view_scroll_trigger` | float | `30.0` | Width in logical pixels of the scrolling-view edge zone used during a Wayland drag-and-drop operation. |
| `niri_dnd_edge_view_scroll_delay_ms` | int | `100` | Delay before file drag-and-drop edge scrolling begins. |
| `niri_dnd_edge_view_scroll_max_speed` | float | `1500.0` | Maximum file drag-and-drop edge-scroll speed in logical pixels per second. Speed increases linearly toward the edge. |
| `niri_dnd_edge_workspace_switch_trigger` | float | `50.0` | Height in logical pixels of the monitor-edge zone used to navigate workspaces during file drag-and-drop. |
| `niri_dnd_edge_workspace_switch_delay_ms` | int | `100` | Delay before drag-and-drop workspace edge navigation begins. |
| `niri_dnd_edge_workspace_switch_max_speed` | float | `1500.0` | Maximum drag-and-drop workspace edge-navigation speed in logical pixels per second. |
| `niri_dnd_hold_to_activate_delay_ms` | int | `750` | Time a file drag must remain over an overview window or workspace before Hymission activates it and closes overview. |
| `toggle_switch_mode` | bool | `1` | Turn `hymission:toggle` into a toggle-only switch session. Intended for modifier-backed bindings such as `ALT+TAB` / `SUPER+TAB`. |
| `switch_toggle_auto_next` | bool | `1` | Toggle switch mode only. When enabled, the first switch-mode `toggle` both opens overview and advances to the next target. |
| `switch_release_key` | string | `Super_L` | Toggle switch mode only. Release of this key commits the current selection and closes the switch session. Supports keysym names such as `Alt_L` / `Super_L` and `code:N`, and release tracking is resilient to missing per-window release events. |
| `gesture_invert_vertical` | bool | `0` | Invert the plugin-managed vertical overview gesture direction. |
| `only_active_workspace` | bool | `1` | Accepted compatibility key. It does not select the previous active-workspace overview; the unified overview manages workspace lanes. |
| `only_active_monitor` | bool | `0` | Accepted compatibility key. The unified overview uses its owner monitor. |
| `show_special` | bool | `0` | Include currently visible special workspaces on the owner monitor in the default scope. |
| `workspace_change_keeps_overview` | bool | `1` | Keep overview open when switching workspace lanes. |
| `damage_tracking_override` | bool | `1` | Temporarily set Hyprland `debug:damage_tracking` to `0` while overview is visible and restore the previous value on close. This can reduce flicker on NVIDIA/multi-monitor setups. |
| `close_special_workspaces_on_open` | bool | `1` | Close any currently visible special workspaces before opening overview. |
| `show_focus_indicator` | bool | `0` | Render selected and hovered preview focus chrome. |

Behavior notes:

- Workspace changes use the dedicated overview-to-overview transition path.
- Toggle switch mode keeps current hover semantics: if `overview_focus_follows_mouse = 1`, moving the pointer can still retarget the final committed selection during the switch session.

### Layer visibility and proxies

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `hide_layers_when_overview` | bool | `1` | Enable configured layer namespace hiding in the workspace-lane overview. |
| `hide_namespace_overview_niri_scrolling` | string | `chromack,wardnc,wardbnc,dashboard,rofi` | Comma-separated ECMAScript patterns for layer surfaces hidden or proxied with workspace lanes. The key name is retained for compatibility. |
| `hide_namespaces_overview` | string | `hypr-dock,waybar,chromack,wardnc,wardbnc,dashboard,rofi` | Additional layer namespace patterns used for empty owner-workspace lane proxies. |
| `hide_bar_animation` | bool | `1` | Enable fade and blur rendering for captured layer proxies while their live surfaces are hidden. |
| `hide_bar_animation_move_multiplier` | float | `0.8` | Movement multiplier for hidden-layer proxy animations, clamped to `0.0` - `2.0`. |
| `hide_bar_animation_scale_divisor` | float | `1.1` | Scale divisor for hidden-layer proxy animations, with a minimum of `1.0`. |
| `hide_bar_animation_blur` | bool | `1` | Enable blur during the layer-proxy transition. |
| `hide_bar_animation_alpha_end` | float | `0.0` | Final opacity of a layer proxy during the transition. Clamped to `0.0` - `1.0`. |

### Legacy workspace-strip settings

The keys `workspace_strip_anchor`, `workspace_strip_empty_mode`, `workspace_strip_thickness`, `workspace_strip_gap`, `hide_bar_when_strip`, and `bar_single_mission_control` remain accepted for compatibility. The unified overview has no separate workspace strip and does not rename workspace entries for bars, so these settings do not select or restore an older overview presentation.

The unified overview shows workspace lanes. Empty lanes follow the configured empty-workspace policy.

All Hyprland layouts share the same owner-monitor workspace-lane overview. Scrolling layouts project Hyprland's live column and tile geometry; their resize, move, swap, and focus actions remain owned by Hyprland. Other layouts map native targets into the same lane geometry and rebuild after normal Hyprland dispatches. `focus_fit_method = 0` centers the focused scrolling column; `focus_fit_method = 1` fits it inside the viewport. Both `hymission:scroll,layout` and Hyprland's official `scrollMove` / Lua `scroll_move` can scroll a scrolling layout while overview is open. The preview-fit caps apply on the scrolling-layout fit path; native-layout lanes use their lane and monitor fit.

Left-dragging a window in this mode uses Hyprland's `binds:drag_threshold`; the translucent preview follows the grabbed point without detaching the tiled window or reflowing its source column. Scrolling-layout lanes show insertion hints for a new column or a position inside an existing column. Native-layout lanes choose the nearest tiled target as a swap target, while floating windows use the mapped drop position. Releasing outside a valid lane or cancelling the drag leaves the layout unchanged. Holding the pointer at a scrolling lane's edge moves its preview camera after the configured delay.

An existing Wayland drag-and-drop operation, such as dragging files from a file manager, can continue after this overview is opened. Overview previews remain compositor activation targets rather than scaled client input surfaces. Holding the drag over a window or workspace activates it and closes overview; release the item after the normal workspace view returns. Holding at a scrolling lane's layout edge pans that workspace, while holding at the top or bottom content edge navigates workspace lanes. DnD hot-corner entry is intentionally disabled, so open overview with a binding while continuing the drag; the DnD entry path is kept separate for a future hot-corner implementation.

### Waybar workspace names

The unified overview leaves Hyprland workspace names unchanged for Waybar. `bar_single_mission_control` remains an accepted compatibility key but does not create a single `Mission Control` entry.

### Debug options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `debug_logs` | bool | `0` | Enable overview debug logging. |
| `debug_surface_logs` | bool | `0` | Enable more verbose surface-level debug logging. |

## Development

For a current implementation walkthrough, including unified workspace lanes, scrolling-layout projection, complex-function explanations, and a catalog of every tracked file, see [`docs/codebase-guide.md`](docs/codebase-guide.md).

Useful commands:

```sh
./build-cmake/hymission-layout-demo
./build-cmake/hymission-layout-demo --list-scenes
./build-cmake/hymission-layout-demo --scene forceall --engine natural --output /tmp/hymission-forceall-natural.svg
./build-cmake/hymission-layout-demo --scene forceall --engine grid --output /tmp/hymission-forceall-grid.svg
./build-cmake/hymission-layout-demo --stress 5000 --seed 1 --output /tmp/hymission-stress-worst.svg
./build-cmake/hymission-mission-layout-test
./build-cmake/hymission-overview-logic-test
hyprctl dispatch hymission:debug_current_layout
```

`hymission-layout-demo` runs the geometry solver without loading the Hyprland plugin. In SVG output, dashed rectangles are source window geometry and solid rectangles are overview targets. Built-in scenes include `forceall`, `default`, `stacked`, `right-biased`, and `workspace-rows`. It also reports gravity, heatmap balance, motion, and x/y inversion metrics; SVG output draws heat cells, the screen center, and the target-area centroid. `--stress` generates random pathological scenes and writes the worst-scoring case for solver tuning.

Project docs:

- [`docs/codebase-guide.md`](docs/codebase-guide.md): current architecture, unified overview and scrolling-layout deep dive, complex functions, and complete file catalog
- [`docs/spec.md`](docs/spec.md): behavior and user-facing semantics
- [`docs/architecture.md`](docs/architecture.md): controller, hooks, and state-machine structure
- [`docs/research.md`](docs/research.md): layout tradeoffs and prior-art notes
- [`docs/workspace_strip_plan.md`](docs/workspace_strip_plan.md): strip, cross-workspace window drag, and external DnD implementation notes
- [`docs/todo.md`](docs/todo.md): current gaps and next steps
- [`devlog/`](devlog): implementation notes for recent iterations

## Credits and attribution

This repository is based on [gfhdhytghd/hymission](https://github.com/gfhdhytghd/hymission). Credit for the original Mission Control overview, foundational plugin architecture, and inherited non-scrolling behavior belongs to the upstream project and its contributors.

My primary contribution is the scrolling-layout projection that became the basis for the unified overview, along with its animation, navigation, workspace-lane, wallpaper-viewport, window-dragging, and external drag-and-drop integration described above.

The project is inspired by Apple Mission Control and references ideas or prior work from [hyprexpo](https://github.com/hyprwm/hyprland-plugins/tree/main/hyprexpo), [hycov](https://github.com/ernestoCruz05/hycov), and [Hyprspace](https://github.com/KZDKM/Hyprspace).

## Notes

- The repository includes a root [`hyprpm.toml`](hyprpm.toml) manifest, which is expected by `hyprpm`.
- For inclusion in the official `hyprland-plugins` repository, Hyprland asks plugin authors to coordinate with the repository maintainer first.
