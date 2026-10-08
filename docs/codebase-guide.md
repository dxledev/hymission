# Hymission Codebase Guide

This is the current implementation guide for the repository. It is written for a portfolio reader who wants the engineering story and for a maintainer who needs to locate a visible behavior.

Older documents in `docs/`, `devlog/`, `.hermes/`, and the repository root remain useful history, but they are not all descriptions of the current implementation.

## 1. Portfolio summary and attribution

Hymission is a compositor plugin that adds a Mission Control-style overview to Hyprland. It does not create a separate shell or move applications into fake containers. It intercepts Hyprland's rendering and input, computes preview geometry, and projects live compositor surfaces into that geometry.

This repository is a fork and adaptation of the public Hymission project. Its original overview architecture, GNOME-inspired layout solver, render-hook strategy, and interaction model come from that lineage. The current overview presents participating workspaces as lanes for every Hyprland layout algorithm. Scrolling layouts use direct column/tile projection; other layouts project native layout targets into the same lane geometry. The `niri_mode` and `only_active_workspace` keys remain accepted for configuration compatibility and do not select the previous overview modes.

The unified implementation includes workspace lanes, empty-workspace viewports, transitions, layout editing, focus reconciliation, window drag, mouse resize, Wayland file drag-and-drop, wallpaper zoom, and render/animation edge-case fixes.

This guide describes what is in the branch; it is not a line-by-line authorship claim. For a portfolio, describe the work as extending and deeply adapting an open-source plugin, then name the unified lane projection and scrolling-layout systems you designed or refined.

## 2. Mental model

The plugin has four layers:

1. `main.cpp` registers configuration, dispatchers, and Lua functions.
2. `OverviewController` owns the live session: state, hooks, input, animation, rendering, and cleanup.
3. Pure geometry modules compute layout slots, hit targets, gesture decisions, and drag insertion targets.
4. Projection modules adapt native Hyprland layout targets to workspace lanes; scrolling algorithms keep their live geometry.

| Path | Geometry authority | Hymission's job |
| --- | --- | --- |
| Native-layout workspace lane | Hyprland layout targets | Project native targets into the workspace's overview lane. |
| Scrolling-layout workspace lane | Hyprland `CScrollingAlgorithm` | Read the live column/tile tape and project it into overview workspace viewports. |
| Floating windows | Hyprland floating targets | Project workspace-relative positions as overlays on their viewport. |

The central rule is:

> Hyprland owns real window and scrolling-layout state. Hymission owns how that state is represented, animated, selected, and edited while the overview is visible.

Breaking that ownership rule produces snapping, stale focus, duplicate animations, or a surface rendered with geometry from the wrong workspace.

## 3. Runtime data flow

```text
dispatcher / key / mouse / touch / gesture
                    |
                    v
             OverviewController
            /        |         \
       build State  update UI  intercept edit
           |          |             |
           v          v             v
    collect scene  selection   Hyprland action
           |          |             |
           +----> preview geometry <-+
                    |
                    v
       render/surface hooks transform
       live Hyprland window surfaces
                    |
                    v
             overview on screen
```

The unified overview has two projection paths. Native-layout lanes map Hyprland layout targets into each workspace lane, while scrolling-layout lanes read column/tile targets, camera offset, and animated window boxes. Edits go back through Hyprland; the overview then rebuilds native lanes from updated targets or retargets scrolling lanes from their current visual frame.

## 4. Coordinate systems

Several difficult functions translate between these spaces:

| Space | Meaning | Common representation |
| --- | --- | --- |
| Native/global | Real compositor-space window rectangle. | `naturalGlobal`, `exitGlobal` |
| Layout target | Hyprland tiled/floating target, possibly animating. | `layoutTarget()->position()` |
| Workspace viewport | Real or frozen usable area for a workspace. | `NiriOverviewViewportSnapshot` |
| Overview target | Where the live surface should appear. | `targetGlobal`, `slot.target` |
| Relayout origin | Visible rectangle captured before an edit. | `relayoutFromGlobal` |
| Workspace transition | Source/target scene interpolation. | `workspaceTransitionRectForWindow()` |
| Legacy strip snapshot | Workspace capture in a thumbnail framebuffer. | `WorkspaceStripEntry::Snapshot` |

When investigating a jump, first ask which space supplied the source rectangle and which subsystem owns the current animation.

## 5. Core state

The major runtime types are declared in `src/overview_controller.hpp`.

### `State`

One complete overview scene. It owns lifecycle phase, owner monitor/workspace, collection policy, participating monitors/workspaces, managed windows, strip entries, empty-workspace placeholders, selection/focus, fullscreen backups, and animation progress.

It is a scene snapshot rather than a thin view of the compositor. The controller can therefore retain a source state, build a target state, and interpolate between them.

### `ManagedWindow`

Wraps a `PHLWINDOW` with native, exit, target, and relayout rectangles; monitor; slot/scale; alpha; and floating/pinned/niri-overlay classification. The Hyprland window remains the render source; this record says how it appears.

### `WorkspaceStripEntry`

Legacy strip state for a workspace thumbnail. It can hold a captured framebuffer, per-window preview metadata, current/relayout rectangles, and active/special/empty/new-workspace flags. The unified overview does not present it as a separate navigation strip.

### `EmptyWorkspacePlaceholder`

Represents a workspace without an ordinary window surface. In native and scrolling-layout lanes it can also act as a backing viewport behind real windows, so “placeholder” does not always mean empty. It supports empty lanes, wallpaper viewports, open/close camera geometry, and retention of a just-vacated workspace.

### `WorkspaceTransition`

Owns an overview-to-overview switch: source and target states, axis/direction/distance, progress, gesture/timed mode, target workspace, and commit state. Until commit, it is the visual authority.

### Interaction sessions

- `GestureSession` opens/closes the overview.
- `ScrollGestureSession` moves the scrolling tape.
- `WorkspaceSwipeGestureContext` borrows Hyprland workspace-swipe input.
- `NiriDragSession` tracks a compositor window drag, layout/workspace target, pointer ratio, and scrolling-lane edge scroll.
- `NiriDndSession` tracks external Wayland data drag, hover activation, scrolling, and workspace edges.

The sessions stay separate because opening the overview, moving a workspace conveyor, scrolling columns, moving a window, and steering protocol DnD have different commit rules.

## 6. Unified overview

### Collection and layout

`loadCollectionPolicy()` applies the default or requested monitor/special-workspace scope, while all scope arguments still use the same owner-monitor lane overview. `only_active_monitor` remains an accepted compatibility setting and does not select another overview mode. `buildState()` resolves workspaces, candidates, fullscreen backups, ordering, and selection.

The unified overview projects Hyprland workspace targets into lanes. The direct-scrolling path reads `CScrollingAlgorithm` geometry; other layouts map native targets into their workspace lane.

`MissionControlLayout::compute()` remains as a legacy standalone layout entrypoint:

1. validate and prepare natural window rectangles;
2. select grid or natural layout;
3. apply row grouping if configured;
4. produce `WindowSlot` targets and scales;
5. preserve caller indices.

The legacy grid solver is GNOME-style row layout: try candidate row counts, score size/space usage, then center the winning rows. The natural solver starts near real screen positions, resolves overlap, spreads sparse arrangements, relieves corner voids, and scores multiple profiles. These solvers remain useful in the layout demo and tests; they do not select the unified overview's projection.

### Rendering

`windowTransformFor()` compares a live window rectangle with `currentPreviewRect()` and returns scale/translation. Surface, UV, texture-box, region, border, and shadow hooks apply the transformation to the live surface tree and decorations. Real layout geometry does not change.

Two custom pass elements keep overview-only drawing at predictable render stages:

- `OverviewWallpaperPassElement` draws the dark backdrop, wallpaper viewports, and backing placeholders behind live window surfaces.
- `OverviewOverlayPassElement` draws hidden-layer proxies, empty placeholders, selection chrome, and drag hints above the appropriate scene content. Its chrome-only form can put selection decoration above a later direct-surface overlay.

### Input and selection

Mouse/touch use preview hit boxes; keyboard navigation uses pure directional-neighbor logic. Selection may synchronize real focus, but remains independent so delayed focus events cannot corrupt the scene.

The unified overview uses workspace lanes in one scene and does not show a separate workspace strip.

## 7. Workspace-lane projection

### Activation and ownership

Every participating workspace is represented by a lane. The projection path follows the workspace's actual Hyprland layout algorithm, independent of the legacy `niri_mode` and `only_active_workspace` values. Empty lanes keep a backing placeholder as the workspace surface.

### Native-layout lane projection

Each frame maps current native-layout window targets uniformly from the workspace work area into its lane. This aspect-preserving transform keeps relative position and size stable while the lane scales; native dispatches remain owned by Hyprland and their updated targets feed the next projection.

### Scrolling-lane projection

The controller reads `CScrollingAlgorithm` column order/widths, tile order/sizes, focus, controller offset, layout direction, work area, and animated geometry. It does not maintain a second authoritative layout. It dispatches edits through Hyprland, then refreshes from Hyprland's result.

`captureNiriOverviewViewports()` freezes monitor/work-area geometry for the session so changing dock/layer reservations cannot alter zoom geometry midway.

`buildState()` creates:

- a lane/backing viewport for each real or synthetic workspace;
- continuous empty IDs if configured;
- projected tiled windows inside each viewport;
- floating overlays positioned relative to their workspace;
- selection/focus ownership for the visually active lane.

For native-layout tiled targets, the projection maps each target uniformly from the workspace work area into its lane, preserving relative placement and size. For scrolling-layout tiled targets, the projection:

1. picks a workspace viewport/base;
2. reads native source geometry;
3. chooses an anchor window/column;
4. scales native offsets into overview offsets;
5. applies preview gaps;
6. overlays floating and pinned windows separately.

Primary/secondary axis helpers make horizontal, vertical, and reversed scrolling share most calculations.

### Per-frame geometry arbitration

`currentPreviewRect()` arbitrates the current visual rectangle across both projection paths. Shared early precedence includes:

1. workspace-transition geometry;
2. interactive open/close gesture;
3. active relayout interpolation.

Native lanes use their mapped layout targets. Later scrolling-lane choices include workspace-transfer or edge-camera live geometry, dynamic tiled geometry, floating-overlay mapping, and ordinary open/active/close interpolation.

On scrolling lanes, the choice between live and goal geometry is deliberate. Hyprland can redirect an animation before cached layout data catches up. A wrong choice produces snap-back, shrink-then-grow, or a one-frame old-workspace flash.

### Focus and camera

Scrolling focus and camera position are related but not identical. Center mode and fit mode behave differently; scrolling past an edge may intentionally release focus while the camera keeps moving; delayed focus events can be older than overview selection.

`syncScrollingWorkspaceSpotOnWindow()`, `syncRealFocusDuringOverview()`, `directNiriFocusedOverviewWindow()`, and edge-camera guards decide when focus should move the camera, when the camera must be preserved, and when a stale event must be ignored.

### Layout edits

`runOverviewEditingDispatcher()` distinguishes native-layout actions from scrolling-layout editing. Native-layout dispatches run through Hyprland and refresh the overview from resulting targets. The scrolling-layout path adapts focus, move, column move/swap, resize, workspace move, and float/tile operations:

1. resolve the visually authoritative workspace/window;
2. settle or retarget active transitions;
3. capture current preview rectangles;
4. prepare real focus without unwanted camera movement;
5. invoke the Hyprland action;
6. inspect the resulting scrolling state;
7. rebuild metadata/state;
8. restore selection/focus invariants;
9. continue or start relayout animation.

Special paths cover edge-camera motion, `movecol`, `swapcol`, resize, silent transfers, floating windows, and interrupted animations because focus, camera, snapshots, and animated goals become visible at different times.

### Workspace transition

A scrolling-layout workspace switch renders source and target lanes together, interpolates them like a conveyor, and commits the real Hyprland workspace at the handoff. Inactive workspaces may be temporarily borrowed for rendering. Transfer guards ensure a moved window uses target-state geometry even if a source snapshot still contains it.

### Wallpaper and hidden layers

Wallpaper zoom captures a configured layer into a framebuffer, crops/scales it for each viewport, optionally draws a dark shadow, and uses custom pass order behind live windows. Hidden bar/layer proxies use a similar capture so the real layer can move/fade out without a visual hole, while selected namespaces remain refreshable.

### Window drag

The window-drag module keeps the layout unchanged while the pointer preview moves. On scrolling lanes it records source column/tile data, builds insertion hints from live columns/tiles, edge-scrolls, and applies a column/tile/workspace/floating move on drop. On native-layout lanes it uses the nearest tiled target as a swap target or maps a floating drop position into the lane. Cancellation or release outside a valid lane leaves the layout unchanged; completed drops restore owner/focus invariants and animate from the release-frame preview.

### Mouse resize

The resize module maps overview pointer movement back to native layout coordinates. For scrolling-layout tiled targets it temporarily centers the target column, recalculates, then restores the camera offset. Native-layout targets use Hyprland's native resize path after mapping pointer coordinates through the lane scale. Floating resize mirrors Hyprland corner, aspect-ratio, size-limit, and snap behavior without transferring workspace focus.

### Wayland file DnD

The DnD module detects the data-device drag, hit-tests windows/lanes, sets Hyprland's DnD pointer focus, supports hold-to-activate, scrolls near view edges, switches lanes near workspace edges, and cleans up on release/close. It steers the existing protocol path; it does not reimplement data transfer.

## 8. Convoluted functions

### `OverviewController::buildState()`

Location: `src/overview_controller_niri_scrolling.cpp`

Purpose: build one complete renderable scene from compositor state.

Read it as nine phases: scope/ownership, collection, ordering, lane preparation, window classification, geometry, placeholders/backgrounds, selection, and final normalization.

Important invariants:

- matching `state.windows` and `state.slots` entries describe the same item;
- the active lane is monitor-local;
- a removed real workspace may remain as a synthetic visual lane;
- borrowing an inactive workspace must not activate it for the user;
- scrolling-layout geometry comes from Hyprland, not the general solver;
- native-layout targets preserve relative position and aspect ratio within their lane.

### `OverviewController::currentPreviewRect()`

Location: `src/overview_controller_niri_scrolling.cpp`

Purpose: return the rectangle drawn on this exact frame.

It is large because it arbitrates transitions, gestures, relayout, live Hyprland animation, resize, edge camera, transfer guards, floating overlays, and ordinary interpolation. Read it as a priority ladder: each early return names a stronger geometry owner.

Invariant: never interpolate twice. Either project Hyprland's live animation or use Hymission's captured relayout, not both.

### `OverviewController::runOverviewEditingDispatcher()`

Location: `src/overview_controller_niri_scrolling.cpp`

Purpose: run a real Hyprland edit while preserving a coherent overview.

Read around the actual dispatch: code before it chooses/captures the correct visual target; code after it classifies the outcome and chooses a refresh/retarget path.

Invariant: act on what the user sees as selected, not stale native focus, while still giving Hyprland the real context required by its action.

### `OverviewController::applyDirectNiriDragTarget()`

Location: `src/overview_controller_niri_drag.cpp`

Purpose: commit a drop to a workspace, scrolling column/tile, native tiled target, or floating position.

On scrolling layouts, it computes and applies a column/tile insertion or workspace move. On native layouts, it swaps with the nearest tiled target or maps a floating drop position. Layout changes happen only when a valid drop is committed; cancellation and invalid drops leave layout geometry unchanged. The handler then restores focus/owner invariants, rebuilds, and animates.

Invariant: dropping a window must not switch the overview owner unless the interaction intends it.

### Workspace transition begin/commit

Location: `src/overview_controller.cpp`

`beginOverviewWorkspaceTransition()` builds the target scene and motion. `commitOverviewWorkspaceTransition()` activates the real workspace, reconciles focus/fullscreen, adopts the target state, and releases render overrides.

Invariant: before commit, transition target state is visually authoritative; after commit, `m_state` and Hyprland's active workspace agree.

### `beginOpen()`, `beginClose()`, and `deactivate()`

Location: `src/overview_controller.cpp`

These form a lifecycle transaction. Open builds state and enables temporary overrides/captures. Close resolves exit focus/geometry and may settle. Deactivate restores hooks, names, fullscreen, focus-follow, animations, timers, proxies, and sessions.

Invariant: every temporary mutation has an unconditional cleanup path, including aborts and empty/failed opens.

### `renderWorkspaceStripSnapshot()`

Location: `src/overview_controller.cpp`

Renders a workspace into an off-screen framebuffer by temporarily installing a render context, drawing layers/windows under strip rules, restoring compositor state, and storing the capture.

Invariant: borrowed monitor/workspace/render state must never leak into the live frame.

### `rebuildVisibleState()`

Location: `src/overview_controller.cpp`

Captures current rectangles, rebuilds, carries compatible snapshots/placeholders, restores selection, installs relayout origins, and decides whether to animate.

Invariant: animation starts from what is currently visible, not the previous cached target.

### Natural layout pipeline

Location: `src/mission_layout.cpp`

`buildNaturalAnchorMap()` preserves screen geography; `buildNaturalItems()` creates solver bodies; `solveNaturalItems()` removes overlap; `spreadNaturalTargets()` and `relieveCornerVoids()` improve balance; `naturalVisualCost()` scores profiles; `computeNaturalLayout()` chooses the winner.

Invariant: targets remain inside the overview area and non-overlapping while retaining useful size and spatial memory.

## 9. File-by-file catalog

This covers every tracked file. Generated `build/`, `build-cmake/`, and `build-meson/` content is ignored.

### Repository and build files

| File | Purpose |
| --- | --- |
| `.gitignore` | Ignores supported build directories; no runtime effect. |
| `.antigravitycli/b5730eb7-2db9-43a4-871a-6df3ffc907d2.json` | Local tool workspace metadata; no build/runtime effect and normally not portfolio material. |
| `AGENTS.md` | Safe Hyprpm/live-reload rules that prevent duplicate plugin instances and compositor instability. |
| `CMakeLists.txt` | Primary build; produces the plugin, demo, and three test programs. Authoritative source list. |
| `meson.build` | Equivalent Meson build; keeps the project portable across two build systems. Its project version is currently `0.3.2`, one revision behind CMake/plugin metadata `0.3.3`. |
| `Makefile` | CMake convenience targets. Hyprpm actions default to dry-run because reload is live/destructive to session stability. |
| `hyprpm.toml` | Hyprpm metadata/build commands and plugin output path. |
| `LICENSE` | GPL-3.0 terms for the fork/derivative work. |
| `logo.svg` | Repository presentation asset showing overview windows. |
| `README.md` | User-facing features, install, dispatchers, Lua, gestures, configuration, and development commands. |
| `debug.log` | Checked-in sample runtime evidence from an older render-visibility investigation; never read by the plugin. |

### Source files

| File | Purpose and overview effect |
| --- | --- |
| `src/main.cpp` | Plugin ABI entrypoint. Registers all options, dispatchers, Lua helpers, defaults, and the controller. |
| `src/mission_layout.hpp` | Pure layout contract: rectangles, inputs, slots, engines, and configuration. |
| `src/mission_layout.cpp` | Standalone grid and natural solvers used by the layout demo and tests. |
| `src/overview_logic.hpp` | Pure policy/geometry declarations for navigation, gestures, transitions, scrolling axes, strip layout, and empty lanes. |
| `src/overview_logic.cpp` | Implements those decisions without Hyprland rendering dependencies. |
| `src/overview_drag_logic.hpp` | Pure axis-normalized column/tile insertion and edge-scroll API. |
| `src/overview_drag_logic.cpp` | Chooses new-column/in-column hints and signed edge velocity. |
| `src/overview_controller.hpp` | Map of the runtime: state/session types, hook API, subsystem methods, overrides, timers, and fields. |
| `src/overview_controller.cpp` | General runtime: lifecycle, hooks, input, gestures, workspace transitions, transforms, surfaces/decorations, layer proxies, fullscreen, selection, strip capture/render, rebuild, and cleanup. |
| `src/overview_controller_niri_scrolling.hpp` | Cross-file scrolling helpers for animation configs, swap repair/trace, ID wrapping, lane retention, and transfer guards. |
| `src/overview_controller_niri_scrolling.cpp` | Main scrolling-layout implementation: scrolling internals, lanes, projection, per-frame geometry, focus/camera, edit dispatchers, empty workspaces, wallpaper viewports, and `buildState()`. |
| `src/overview_controller_niri_drag.cpp` | Cross-layout compositor-window drag, with scrolling insertion/edge-scroll and native target-swap behavior. |
| `src/overview_controller_niri_resize.cpp` | Scaled-preview mouse resize with native constraints and camera/workspace preservation. |
| `src/overview_controller_niri_dnd.cpp` | External Wayland DnD surface focus, hold activation, view/workspace edge actions, timer, release, and cleanup. |

### Tools and tests

| File | Purpose |
| --- | --- |
| `tools/layout_demo.cpp` | Standalone layout laboratory with scenes, random stress cases, metrics, and annotated SVG output. |
| `tools/mission_layout_test.cpp` | Tests scale, containment, overlap, grouping, ordering, emphasis, and both layout engines. |
| `tools/overview_logic_test.cpp` | Tests navigation, interpolation, gesture/transition decisions, lane geometry, parsing, and scrolling-layout helpers. |
| `tools/overview_drag_logic_test.cpp` | Tests insertion kind/location and drag edge velocity. |

### Reference and historical documents

| File | Purpose/status |
| --- | --- |
| `docs/codebase-guide.md` | Current code-oriented guide and complete file catalog. Start here. |
| `docs/research.md` | English attribution/influences from GNOME Shell, Hyprspace, hycov, hyprexpo, KWin, niri, and Hyprland. |
| `docs/spec.md` | English product-level behavior contract for collection, projection, input, transitions, direct-niri, and DnD. |
| `docs/architecture.md` | Concise English module, state, runtime-flow, hook, and verification map. |
| `docs/workspace_strip_plan.md` | English implementation note for workspace thumbnails, window dragging, and external DnD. |
| `docs/todo.md` | English list of remaining opportunities and deliberate non-goals. |
| `wallpaper_zoom_plan.md` | Scratch/design notes for wallpaper viewport zoom; no current API authority. |
| `.hermes/plans/2025-06-12_153000-niri-workspace-switch-animation-fix.md` | Generated detailed plan for niri workspace animation/focus conflicts; current code has evolved beyond it. |

### Development logs

| File | Investigation recorded |
| --- | --- |
| `devlog/2026-03-07-hyprland-crash.md` | Workspace-transition crash, evidence, root cause, fix, and risk. |
| `devlog/2026-03-07-recommand-gesture-and-strip-slide.md` | `recommand` gestures, scope transfer, force-all origins, and strip slide. |
| `devlog/2026-03-07-strip-click-input-fallback.md` | Corrupt/missing strip-click callbacks and primary-button fallback. |
| `devlog/2026-03-07-strip-refresh-and-input-regression.md` | Strip refresh/preview semantics, input/reopen regression, and freeze. |
| `devlog/2026-03-07-strip-transition-polish.md` | Overlay ordering, focus gating, snapshot carry-over, and new empty workspaces. |
| `devlog/2026-03-08-gesture-exit-lock-and-layout-coverage.md` | Gesture exit/locking, empty-strip behavior, retries, and layout coverage. |
| `devlog/2026-03-08-strip-bar-handoff-and-crop-fix.md` | Hidden-bar proxy handoff and monitor-capture crop fix. |
| `devlog/2026-03-09-expand-selected-live-focus-stability.md` | Selected emphasis, local push-away, focus response, and scope anchoring. |
| `devlog/2026-03-09-multi-workspace-recent-first-sorting.md` | MRU tracking and scoped recent-first ordering. |
| `devlog/2026-03-09-toggle-switch-mode-and-release-tracking.md` | Toggle task-switch session, circular selection, and modifier release. |

## 10. Find code by visible behavior

| Behavior | Start here |
| --- | --- |
| Load failure or missing option | `src/main.cpp`, then build files |
| Workspace-lane projection | `buildState()` in `src/overview_controller_niri_scrolling.cpp` and native target mapping |
| Standalone grid/natural solver | `src/mission_layout.cpp`, `tools/layout_demo.cpp` |
| Wrong mouse/keyboard target | `src/overview_logic.cpp`, input handlers |
| Surface/border/shadow/blur/clipping | `windowTransformFor()` and render/surface hooks |
| Open/close/cleanup | `beginOpen()`, `beginClose()`, `updateAnimation()`, `deactivate()` |
| Workspace conveyor | transition functions in `src/overview_controller.cpp` |
| Legacy strip snapshot refresh | `renderWorkspaceStripSnapshot()` and layer proxies |
| Workspace-lane positions | `buildState()` and `currentPreviewRect()` |
| Edit snaps/wrong focus | `runOverviewEditingDispatcher()` and focus/camera helpers |
| Empty/wallpaper lane | placeholder builders and `renderNiriWorkspaceBackgrounds()` |
| Window drag | niri drag module plus pure drag logic |
| Mouse resize | niri resize module, then `currentPreviewRect()` |
| File DnD | niri DnD module |
| Runtime-only regression | `/tmp/hymission-debug.log` with `debug_logs = 1` |

## 11. Reading and verification

Suggested order:

1. README features and niri options.
2. Sections 2–7 here.
3. The three pure headers.
4. State structs in `overview_controller.hpp`.
5. `main.cpp`.
6. General open/render/close paths.
7. Workspace-lane activation, `buildState()`, and `currentPreviewRect()`.
8. Drag, resize, and DnD as separate state machines.
9. Devlogs for evidence-based debugging examples.

Repeatable checks:

```bash
cmake -DCMAKE_BUILD_TYPE=Release -B build-cmake
cmake --build build-cmake -j "$(nproc)"
./build-cmake/hymission-mission-layout-test
./build-cmake/hymission-overview-logic-test
./build-cmake/hymission-overview-drag-logic-test
```

Pure tests cannot prove compositor-hook timing. Direct-niri rendering, focus, transfers, layer capture, and DnD need a live Hyprland session and targeted logs.

Do not automatically reload the Hyprpm-managed plugin in an active session. Follow `AGENTS.md`: commit intended work, then let the user run `hyprpm update` from a safe context.
