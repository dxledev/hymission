# Hymission Behavior Specification

This document defines product-level behavior. Configuration names and defaults
are listed in the main [`README.md`](../README.md); implementation ownership is
described in [`codebase-guide.md`](codebase-guide.md) and inline source comments.

## 1. Product definition

Hymission provides a Mission Control-style overview inside Hyprland:

- dispatcher arguments and configuration determine the collection scope;
- every participating window is shown as an aspect-preserving preview;
- normal overview tries to retain recognizable spatial relationships;
- direct-niri mode presents scrolling-layout workspaces as zoomed lanes;
- leaving overview does not alter application logical resolution.

Hymission is not a replacement tiling layout, desktop shell, or separate
workspace manager. Hyprland remains authoritative for native compositor state.

## 2. Collection scope

The supported scope requests are:

- default: use configured monitor/workspace policy;
- `onlycurrentworkspace`: show the anchor monitor's active regular workspace;
- `forceall`: show all regular workspaces on participating monitors and visible
  special workspaces.

An eligible ordinary window must be mapped, visible, in scope, have valid render
geometry, and not be a desktop component. Visible pinned floating windows belong
to their current monitor even if their stored workspace refers to an older one.
Special-workspace windows participate only when policy includes them.

Popups and subsurfaces follow their owner surface; they are not independent
overview entries.

## 3. Preview geometry

### Normal overview

The layout engine receives:

- each window's natural monitor-relative rectangle;
- the usable overview area;
- `LayoutConfig`.

It returns a target rectangle and uniform scale for every input. Previews preserve
aspect ratio, remain uncropped, respect configured scale limits, and retain the
caller's stable index.

Grid mode evaluates row counts and favors readable scale before unused-space
reduction. Natural mode begins near original centers, resolves overlap, and may
fall back to grid when its constraints cannot place every window reliably.
Forced workspace row groups keep each workspace in a distinct row.

### Direct-niri scrolling overview

When niri mode, active-workspace scope, and Hyprland's scrolling layout apply,
Hymission reads the live column/tile order, widths, focus, camera offset, work
area, and animated geometry. It projects that layout into zoomed workspace lanes.

The plugin must not maintain a second authoritative scrolling layout. Edits are
performed through Hyprland and the overview is refreshed from the result.
Floating and pinned windows are projected as overlays rather than tiles.

Empty workspaces receive a backing viewport so they remain visible and usable as
navigation or drop targets.

## 4. Rendering

Overview rendering is compositor-side:

- clients remain in their real native layout;
- surface boxes, regions, textures, decorations, borders, and shadows are
  transformed into preview geometry;
- wallpaper/backing elements render below window surfaces;
- selection chrome, drag hints, and navigation UI render at controlled overlay
  stages;
- closing restores any temporary layer, animation, scanout, focus-following, or
  fullscreen overrides.

A live surface must use geometry from the same workspace scene that owns it.
During a workspace transition, source and target scenes remain distinct until the
native handoff is committed.

## 5. Input

### Dispatchers

- `hymission:open [scope]` opens or rebuilds to the requested scope.
- `hymission:close` exits.
- `hymission:toggle [scope]` opens or closes, except during configured switch
  sessions where repeated activation advances selection.
- `hymission:debug_current_layout` reports layout without entering overview.
- `hymission:workspace` routes workspace activation through overview transition
  handling while visible.

Unknown scope arguments must return an error rather than silently changing scope.

### Mouse and touch

Pointer hit testing uses the visible preview rectangle, not the native window
rectangle. Clicking a preview selects/activates it. Workspace navigation surfaces
can be clicked when present. Direct-niri window dragging begins only after the
native drag threshold and commits through Hyprland.

Mouse resize maps overview pointer movement back into native geometry. It must
preserve the previously focused window and scrolling camera unless the resize
operation itself requires a focus/camera change.

### Keyboard

Directional keys choose the nearest candidate in that direction. Cyclic switch
mode advances through stable preview order. Escape aborts; activation keys commit
the selected target and close.

### Gestures

Official Hyprland gesture syntax is required. Open/close gestures track finger
progress, allow reversal before release, and use distance plus velocity to commit
or cancel. `recommand` can close one scope, cross a deliberate hidden gap, and
open the opposite scope only in the permitted direction.

Workspace gestures can drive overview-to-overview transitions when the current
scope allows workspace switching. The intermediate frame must not expose
Hyprland's normal workspace animation.

`hymission:scroll,layout` and official `scrollMove` operate on the scrolling
layout's primary axis. Hymission refreshes direct-niri projection after native
scroll movement.

## 6. Changes while overview is visible

Window open, close, move, title, workspace, monitor, and layout changes must
either refresh metadata or rebuild the scene. If membership remains stable,
existing order, selection, monitor ownership, and current visual rectangles
should be preserved.

An edit that occurs during a workspace transition is classified as:

- run now when independent of the transition;
- retarget after committing the applicable transition;
- defer until the transition's target scene becomes authoritative.

Relayout animation begins at the current visible rectangle. It must not jump back
to a cached pre-edit endpoint.

## 7. Workspace transitions

A transition owns source and target scene snapshots, axis, direction, distance,
progress, target workspace identity, and commit mode. The visual scenes move
together. Native workspace activation occurs at a controlled handoff, after which
focus, scrolling camera, placeholders, and retained lane state are reconciled.

An interrupted or retargeted transition freezes its current visible frame before
starting another animation.

## 8. Direct-niri focus and camera

Overview selection, native focus, and scrolling-camera anchor are related but
separate state:

- stale native focus events must not replace newer overview selection;
- fit and center camera policies must preserve their distinct behavior;
- scroll-past edge-camera state may intentionally have no centered focused tile;
- `movefocus` must explicitly synchronize the scrolling controller while
  Hyprland's normal `follow_focus` is suppressed;
- workspace transfer and resize guards may temporarily preserve camera geometry.

## 9. Drag and drop

Window drops resolve to a new column, a tile position in a column, or a floating
position. Cross-workspace drops may create a workspace. Drop commit preserves a
valid overview owner and focus, repairs scrolling target membership, and animates
from the release preview.

External Wayland data drag-and-drop keeps protocol surface focus valid while
supporting delayed hover activation, scrolling-tape edge movement, and workspace
edge switching.

## 10. Multi-monitor behavior

Each managed preview belongs to a participating monitor and is clipped/projected
within that monitor's overview space. Pinned windows do not leak to unrelated
monitors. A workspace transition is monitor-local, while force-all overview may
render several monitors simultaneously.

## 11. Acceptance criteria

- Opening and closing do not permanently mutate native layout geometry.
- Every eligible input window receives one stable preview entry.
- Preview content preserves aspect ratio and remains clickable at its visible
  location.
- Normal overview and direct-niri mode retain separate geometry ownership.
- Direct-niri column/tile order matches Hyprland's live scrolling layout.
- Focus, camera, and selection remain coherent after edits and workspace changes.
- Workspace transition frames contain only the intended overview scenes.
- All temporary compositor overrides are restored on normal, aborted, and
  interrupted exits.
- Pure layout/logic tests pass; compositor-sensitive paths are manually verified
  against the supported Hyprland build.

## 12. Non-goals

- Independent popup entries.
- Search/filter UI.
- Group or stack expansion UI.
- A complete task manager or desktop shell.
- Client-side resize as the primary preview mechanism.
