#pragma once

// Shared coordination points for the direct-niri scrolling implementation.
//
// The controller remains the state owner. This narrow detail namespace holds
// process-wide repair/trace flags and helpers that must also be reached by the
// drag controller without exposing them as public plugin API.

#include "overview_controller.hpp"

namespace hymission::niri_scrolling_detail {

// True only while a single-workspace strip snapshot is borrowing scene state.
extern bool stripSnapshotSingleWorkspaceOnly;

// Resolve Hyprland animation configurations used for direct-niri relayouts.
SP<Hyprutils::Animation::SAnimationPropertyConfig> windowsMoveAnimationConfig();
SP<Hyprutils::Animation::SAnimationPropertyConfig> workspaceAnimationConfig();

// Narrow diagnostics and one-shot repair state for two-column swap regressions.
void armTwoColumnSwapTrace(const PHLWORKSPACE& workspace);
bool twoColumnSwapTraceActive(const PHLWORKSPACE& workspace);
bool consumeTwoColumnSwapPreviewTrace(const PHLWORKSPACE& workspace);
void armPendingTwoColumnSwapRepair(const PHLWORKSPACE& workspace);
void clearPendingTwoColumnSwapRepair(const PHLWORKSPACE& workspace);
bool consumePendingTwoColumnSwapRepair(const PHLWORKSPACE& workspace);

// Cross-file lifecycle and workspace-lane coordination.
bool isActiveController(const OverviewController* controller);
bool shouldWrapWorkspaceIds(WORKSPACEID targetId, WORKSPACEID currentId);
void retainDirectNiriWorkspaceLaneForDrag(const PHLMONITOR& monitor, const PHLWORKSPACE& workspace);
void armDirectNiriWorkspaceTransferRenderGuard(const PHLWINDOW& window);

} // namespace hymission::niri_scrolling_detail
