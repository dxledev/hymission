# Workspace Strip and Cross-Workspace Drag

This was originally a forward-looking plan. It is now an English implementation
note because the workspace strip and direct-niri drag paths exist in this branch.

## Current behavior

The standard single-workspace overview can reserve a top, left, or right band for
workspace thumbnails. Entries may represent existing, continuous empty, special,
or create-new workspaces. Clicking an entry starts an overview-to-overview
transition when policy allows it.

Direct-niri scrolling single-workspace mode normally suppresses the separate
strip. Workspace lanes and their backing surfaces form one continuous scene, so
navigation and drag targeting operate on those lanes instead.

## State and geometry

`WorkspaceStripEntry` stores workspace identity, monitor, target rectangle,
snapshot framebuffer, fallback window metadata, and activity flags. Pure helpers
in `overview_logic.cpp` reserve the strip band, expand workspace IDs, lay out
thumbnail slots, and perform hit testing.

`renderWorkspaceStripSnapshot()` temporarily renders an eligible workspace into a
private framebuffer. The previous snapshot remains valid until replacement
succeeds, preventing inactive direct-niri thumbnails from flashing blank.

## Window dragging

Direct-niri window dragging uses three layers:

1. `overview_drag_logic.cpp` computes a new-column or in-column target.
2. `overview_controller_niri_drag.cpp` tracks pointer state and renders the hint.
3. Drop commit updates Hyprland, repairs scrolling membership, preserves valid
   focus/workspace ownership, and retargets animation from the release rectangle.

Cross-workspace drops may create an empty workspace, retain a temporarily vacated
lane, or transition the overview owner. Floating drops remain overlays rather
than scrolling tiles.

## External file drag-and-drop

`overview_controller_niri_dnd.cpp` is a separate state machine for Wayland
data-device drags. It preserves protocol surface focus while adding delayed hover
activation, viewport edge scrolling, and workspace-edge switching.

## Configuration

User-facing strip and drag options are documented in the main
[`README.md`](../README.md) and registered in `src/main.cpp`.

## Verification

Pure insertion and edge-scroll behavior is tested by
`tools/overview_drag_logic_test.cpp`. Snapshot rendering, native drag ownership,
cross-workspace focus, and external DnD still need compositor-level manual tests.
