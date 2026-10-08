#include "overview_controller.hpp"

#include <cmath>

#include <hyprland/src/desktop/Workspace.hpp>

namespace hymission {
namespace {

Rect nativeCameraWindowRect(const PHLWINDOW& window, bool goal) {
    Vector2D position = goal ? hyprland_compat::windowPositionAnimation(window)->goal() : hyprland_compat::windowPositionAnimation(window)->value();
    const Vector2D size = goal ? hyprland_compat::windowSizeAnimation(window)->goal() : hyprland_compat::windowSizeAnimation(window)->value();
    if (window->m_isFloating)
        position += window->m_floatingOffset;
    return {position.x, position.y, size.x, size.y};
}

bool cameraRectStable(const Rect& previous, const Rect& current) {
    constexpr double epsilon = 0.75;
    return std::abs(previous.x - current.x) <= epsilon && std::abs(previous.y - current.y) <= epsilon &&
        std::abs(previous.width - current.width) <= epsilon && std::abs(previous.height - current.height) <= epsilon;
}

}

bool OverviewController::applyNativeLayoutCameraGeometry(const PHLWORKSPACE& workspace, bool opening, bool* stable) {
    if (!workspace || !workspace->m_space || isScrollingWorkspace(workspace) || !usesDirectNiriScrollingOverview(m_state))
        return false;

    const auto monitor = workspace->m_monitor.lock();
    const auto* anchor = directNiriWorkspaceViewportPlaceholder(workspace->m_id, monitor);
    if (!monitor || !anchor)
        return false;

    const Rect desktopViewport = niriOverviewViewportForWorkspace(workspace);
    const Rect overviewViewport = anchor->targetGlobal;
    if (desktopViewport.width <= 1.0 || desktopViewport.height <= 1.0 || overviewViewport.width <= 1.0 || overviewViewport.height <= 1.0)
        return false;

    for (auto& managed : m_state.windows) {
        if (!managed.window || !managed.window->m_isMapped || managed.targetMonitor != monitor)
            continue;

        const bool destinationWindow = managed.window->m_pinned || managed.isPinned || managed.window->m_workspace == workspace;
        if (opening && destinationWindow)
            continue;

        Rect endpoint = transformLiveOverviewRect(managed.targetGlobal, overviewViewport, desktopViewport);
        if (!opening && destinationWindow)
            endpoint = nativeCameraWindowRect(managed.window, shouldPreferGoalExitGeometry(managed.window) || m_state.exitFullscreenReapplied);

        if (opening)
            managed.naturalGlobal = endpoint;
        else if (stable && !cameraRectStable(managed.exitGlobal, endpoint))
            *stable = false;
        managed.exitGlobal = endpoint;
    }

    for (auto& placeholder : m_state.emptyWorkspacePlaceholders) {
        if (placeholder.monitor != monitor)
            continue;

        const Rect endpoint = transformLiveOverviewRect(placeholder.targetGlobal, overviewViewport, desktopViewport);
        if (opening)
            placeholder.naturalGlobal = endpoint;
        placeholder.exitGlobal = endpoint;
    }

    return true;
}

bool OverviewController::applyNativeLayoutCameraOpenGeometry() {
    return applyNativeLayoutCameraGeometry(m_state.ownerWorkspace, true);
}

bool OverviewController::applyNativeLayoutCameraExitGeometry(const PHLWINDOW& window, const PHLWORKSPACE& workspace, bool* stable) {
    const auto destination = workspace ? workspace : (window && !window->m_pinned ? window->m_workspace : m_state.ownerWorkspace);
    return applyNativeLayoutCameraGeometry(destination, false, stable);
}

void OverviewController::prepareNativeLayoutCameraReopenGeometry() {
    const auto window = resolveExitFocus(CloseMode::Normal);
    const auto workspace = resolveExitWorkspace(CloseMode::Normal);
    const auto destination = workspace ? workspace : (window && !window->m_pinned ? window->m_workspace : m_state.ownerWorkspace);
    if (!destination || isScrollingWorkspace(destination) || !usesDirectNiriScrollingOverview(m_state))
        return;

    const auto monitor = destination->m_monitor.lock();
    for (auto& managed : m_state.windows) {
        if (managed.targetMonitor == monitor)
            managed.naturalGlobal = managed.exitGlobal;
    }
    for (auto& placeholder : m_state.emptyWorkspacePlaceholders) {
        if (placeholder.monitor == monitor)
            placeholder.naturalGlobal = placeholder.exitGlobal;
    }
}

}
