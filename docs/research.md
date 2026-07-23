# Research Notes

This document records the design influences behind Hymission. It is not an
authorship claim or a current implementation specification.

## Sources considered

### GNOME Shell overview

The normal overview layout borrows the broad idea of evaluating row
arrangements, prioritizing readable preview size, and preserving enough spatial
order for users to recognize windows. Hymission does not copy GNOME's shell,
workspace model, or actor system; its previews are Hyprland compositor surfaces.

### Hyprspace

Hyprspace demonstrated practical Hyprland render-hook integration and informed
the feasibility of compositor-side overview previews. Hymission has its own
controller, geometry, input, gesture, workspace-transition, and direct-niri
systems.

### Hycov and hyprexpo

These projects provided useful comparisons for overview interaction, plugin
packaging, and the tradeoffs between temporary layouts and render projection.
Hymission keeps the real Hyprland layout authoritative instead of moving clients
into an overview-only layout.

### KWin Expo

KWin's overview is useful prior art for natural placement, position preservation,
and visual continuity. The local natural solver uses those goals as evaluation
criteria, but is a small standalone C++ geometry implementation.

### Niri and Hyprland scrolling layout

The direct-niri branch is based on the interaction model of a scrolling,
column-oriented desktop. Its implementation reads and edits Hyprland's
`CScrollingAlgorithm`; it does not embed niri code or create an independent
layout authority.

## Decisions derived from the research

- Preserve client aspect ratios and avoid client resize as the preview mechanism.
- Prefer readable previews over mechanically filling every pixel.
- Keep familiar relative positions when the natural solver can do so reliably.
- Fall back to deterministic row layout when natural constraints fail.
- Separate pure geometry from compositor hooks.
- In direct-niri mode, project the live scrolling tape instead of approximating
  it with unrelated overview cards.
- Animate from the currently visible frame after edits to prevent snap-back.

## Mapping to source

- Row and natural solvers: `src/mission_layout.cpp`
- Render projection and controller integration: `src/overview_controller.cpp`
- Direct-niri scrolling projection: `src/overview_controller_niri_scrolling.cpp`
- Pure interaction decisions: `src/overview_logic.cpp`
- Drag insertion geometry: `src/overview_drag_logic.cpp`

For a fuller component map, see [`codebase-guide.md`](codebase-guide.md).
