# Hymission Architecture

This is a concise architecture map. For the current implementation, unified
overview deep dive, complex-function explanations, and complete file catalog, see
[`codebase-guide.md`](codebase-guide.md). The source files now also carry module
headers and comments beside the difficult orchestration functions.

## Design principles

- Hyprland owns real windows, workspaces, layouts, focus, and scrolling columns.
- Hymission owns the temporary overview scene, preview projection, animation,
  selection, and input routing.
- Native layout targets are projected into workspace lanes.
- Scrolling-layout geometry is projected directly from Hyprland's live algorithm.
- Pure policy and geometry remain independent of Hyprland so they can be tested.

## Modules

### Layout and pure logic

- `src/mission_layout.{hpp,cpp}` retains the standalone grid and natural layout solvers used by the layout demo and tests.
- `src/overview_logic.{hpp,cpp}` contains hit testing, navigation, gesture
  decisions, lane projection, strip geometry, and scrolling-layout scale helpers.
- `src/overview_drag_logic.{hpp,cpp}` computes scrolling-layout insertion targets
  and edge-scroll velocity.

### Runtime controller

`src/overview_controller.hpp` declares the complete session state and integration
surface. `src/overview_controller.cpp` implements lifecycle, hooks, standard
overview rendering, workspace transitions, input, and workspace snapshots.

Direct-niri behavior is split by responsibility:

- `overview_controller_niri_scrolling.cpp`: lane projection, camera/focus,
  geometry arbitration, edits, wallpaper, and scrolling transitions.
- `overview_controller_niri_drag.cpp`: window dragging and drop commit.
- `overview_controller_niri_resize.cpp`: mouse-resize coordinate adaptation.
- `overview_controller_niri_dnd.cpp`: external Wayland data drag-and-drop.

### Plugin entry and tests

`src/main.cpp` registers configuration, dispatchers, Lua bindings, and the single
controller. Files under `tools/` exercise compositor-independent logic.

## Runtime flow

1. A dispatcher or gesture calls the controller.
2. `buildState()` collects compositor objects and constructs a renderable scene.
3. Each workspace is projected into a lane; normal layouts use native targets
   and scrolling layouts use Hyprland's live scrolling geometry.
4. render hooks transform live surfaces without resizing the client.
5. input operates on the projected rectangles.
6. edits are sent back through Hyprland, then the scene is rebuilt or retargeted
   from its current visible frame.
7. closing restores all temporary compositor overrides.

## State model

`State` is a self-contained scene snapshot. It contains managed windows,
workspaces, monitors, selection, fullscreen backups, strip entries, empty
workspace placeholders, and animation state. A workspace transition retains a
source and target `State`, allowing both scenes to render until native workspace
activation is committed.

Interaction state is intentionally separate:

- `GestureSession`: overview open/close and two-sided `recommand`.
- `ScrollGestureSession`: scrolling-layout tape movement.
- `WorkspaceTransition`: overview-to-overview workspace movement.
- `NiriDragSession`: compositor window drag.
- `NiriDndSession`: Wayland protocol data drag.

## Hook surface

The plugin intercepts render stages, window surface boxes and regions,
decorations, layer rendering, mouse/keyboard/touch events, trackpad gestures,
workspace changes, selected dispatchers, and selected Hyprland actions. Hook
callbacks are adapters; lifecycle and policy remain in the controller.

## Verification boundary

Pure layout, gesture, strip, and drag geometry have standalone tests. Render-hook
compatibility, live compositor animation, multi-monitor interaction, and protocol
DnD still require manual testing inside the matching Hyprland version.
