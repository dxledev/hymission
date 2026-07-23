# Hymission TODO

This list contains remaining product opportunities, not promises or acceptance
criteria. Several items from the original plan—workspace strips, window dragging,
workspace transitions, direct-niri editing, mouse resize, and Wayland DnD—are
already implemented in this branch.

## Candidate work

- Add automated coverage for render-hook transformations and decoration order.
- Add integration coverage for real multi-monitor workspace transitions.
- Add repeatable compositor tests for trackpad interruption and reversal.
- Add protocol-level Wayland DnD tests.
- Reduce the size of `OverviewController` by extracting cohesive state machines
  without duplicating geometry ownership.
- Replace remaining runtime-sensitive compatibility probes when Hyprland exposes
  stable public APIs.
- Review and reconcile CMake/Meson version metadata.
- Continue pruning debug-only workarounds after their upstream timing behavior is
  proven stable.

## Deliberate non-goals

- Replacing Hyprland's window layout.
- Changing application logical resolution for preview rendering.
- Building a complete desktop shell, search system, or task manager.
- Claiming the upstream/forked architecture as original portfolio work.

When adding work, keep direct-niri changes scoped to the applicable mode and
preserve normal overview behavior.
