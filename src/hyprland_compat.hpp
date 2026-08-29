#pragma once

#include <optional>
#include <string>
#include <vector>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>
#include <hyprland/src/desktop/view/Window.hpp>

#if __has_include(<hyprland/src/state/WorkspaceState.hpp>)
#define HYM_HYPRLAND_0_56 1
#include <hyprland/src/animation/AnimationManager.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/desktop/state/LayerState.hpp>
#include <hyprland/src/desktop/state/ViewState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/pointer/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#else
#define HYM_HYPRLAND_0_56 0
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/cursor/CursorShapeOverrideController.hpp>
#endif

namespace hymission::hyprland_compat {

#if HYM_HYPRLAND_0_56
using FullscreenMode = Fullscreen::eFullscreenMode;
inline constexpr FullscreenMode FULLSCREEN_NONE = Fullscreen::FSMODE_NONE;
inline constexpr FullscreenMode FULLSCREEN_FULL = Fullscreen::FSMODE_FULLSCREEN;
#else
using FullscreenMode = ::eFullscreenMode;
inline constexpr FullscreenMode FULLSCREEN_NONE = ::FSMODE_NONE;
inline constexpr FullscreenMode FULLSCREEN_FULL = ::FSMODE_FULLSCREEN;
#endif

inline const std::vector<PHLWINDOW>& windows() {
#if HYM_HYPRLAND_0_56
    return Desktop::windowState()->windows();
#else
    return g_pCompositor->m_windows;
#endif
}

inline const std::vector<PHLLS>& layers() {
#if HYM_HYPRLAND_0_56
    return Desktop::layerState()->layers();
#else
    return g_pCompositor->m_layers;
#endif
}

inline const std::vector<PHLMONITOR>& monitors() {
#if HYM_HYPRLAND_0_56
    return State::monitorState()->monitors();
#else
    return g_pCompositor->m_monitors;
#endif
}

template <typename T, const std::vector<T>& (*Items)()>
class CollectionView {
  public:
    auto begin() const {
        return Items().begin();
    }

    auto end() const {
        return Items().end();
    }

    std::size_t size() const {
        return Items().size();
    }
};

class CompositorFacade {
  public:
    CollectionView<PHLWINDOW, windows>   m_windows;
    CollectionView<PHLLS, layers>        m_layers;
    CollectionView<PHLMONITOR, monitors> m_monitors;

    std::vector<PHLWORKSPACE> getWorkspaces() const {
        return getWorkspacesCopy();
    }

    std::vector<PHLWORKSPACE> getWorkspacesCopy() const {
#if HYM_HYPRLAND_0_56
        return State::workspaceState()->workspacesCopy();
#else
        return g_pCompositor->getWorkspacesCopy();
#endif
    }

    PHLWORKSPACE getWorkspaceByID(WORKSPACEID id) const {
#if HYM_HYPRLAND_0_56
        return State::workspaceState()->query().id(id).run();
#else
        return g_pCompositor->getWorkspaceByID(id);
#endif
    }

    PHLWORKSPACE createNewWorkspace(WORKSPACEID id, MONITORID monitorId, const std::string& name, bool isEmpty = true) const {
#if HYM_HYPRLAND_0_56
        return State::workspaceState()->create(id, monitorId, name, isEmpty);
#else
        return g_pCompositor->createNewWorkspace(id, monitorId, name, isEmpty);
#endif
    }

    PHLMONITOR getMonitorFromID(MONITORID id) const {
#if HYM_HYPRLAND_0_56
        return State::monitorState()->query().id(id).run();
#else
        return g_pCompositor->getMonitorFromID(id);
#endif
    }

    PHLMONITOR getMonitorFromName(const std::string& name) const {
#if HYM_HYPRLAND_0_56
        return State::monitorState()->query().name(name).run();
#else
        return g_pCompositor->getMonitorFromName(name);
#endif
    }

    PHLMONITOR getMonitorFromVector(const Vector2D& point) const {
#if HYM_HYPRLAND_0_56
        return State::monitorState()->query().vec(point).run();
#else
        return g_pCompositor->getMonitorFromVector(point);
#endif
    }

    PHLMONITOR getMonitorFromCursor() const {
#if HYM_HYPRLAND_0_56
        return getMonitorFromVector(Pointer::mgr()->position());
#else
        return g_pCompositor->getMonitorFromCursor();
#endif
    }

    PHLWINDOW getWindowByRegex(const std::string& selector) const {
#if HYM_HYPRLAND_0_56
        return Desktop::viewState()->query().selector(selector).runWindow();
#else
        return g_pCompositor->getWindowByRegex(selector);
#endif
    }

    void moveWindowToWorkspaceSafe(const PHLWINDOW& window, const PHLWORKSPACE& workspace) const {
#if HYM_HYPRLAND_0_56
        Desktop::globalWindowController()->moveWindowToWorkspace(window, workspace);
#else
        g_pCompositor->moveWindowToWorkspaceSafe(window, workspace);
#endif
    }

    void changeWindowZOrder(const PHLWINDOW& window, bool top) const {
#if HYM_HYPRLAND_0_56
        if (top)
            Desktop::windowState()->raise(window);
        else
            Desktop::windowState()->lower(window);
#else
        g_pCompositor->changeWindowZOrder(window, top);
#endif
    }

    void setWindowFullscreenInternal(const PHLWINDOW& window, FullscreenMode mode) const {
#if HYM_HYPRLAND_0_56
        Fullscreen::controller()->setFullscreenMode(window, mode, std::nullopt, true);
#else
        g_pCompositor->setWindowFullscreenInternal(window, mode);
#endif
    }

    void scheduleFrameForMonitor(const PHLMONITOR& monitor) const {
#if HYM_HYPRLAND_0_56
        if (monitor)
            monitor->scheduleFrame();
#else
        g_pCompositor->scheduleFrameForMonitor(monitor);
#endif
    }

    void warpCursorTo(const Vector2D& point) const {
#if HYM_HYPRLAND_0_56
        Pointer::mgr()->warpTo(point);
#else
        g_pCompositor->warpCursorTo(point);
#endif
    }
};

inline CompositorFacade* compositor() {
    if (!g_pCompositor)
        return nullptr;

    static CompositorFacade facade;
    return &facade;
}

inline auto* animationManager() {
#if HYM_HYPRLAND_0_56
    return Animation::mgr().get();
#else
    return g_pAnimationManager.get();
#endif
}

inline PHLANIMVAR<Vector2D>& windowPositionAnimation(const PHLWINDOW& window) {
#if HYM_HYPRLAND_0_56
    return window->positionAnimation();
#else
    return window->m_realPosition;
#endif
}

inline PHLANIMVAR<Vector2D>& windowSizeAnimation(const PHLWINDOW& window) {
#if HYM_HYPRLAND_0_56
    return window->sizeAnimation();
#else
    return window->m_realSize;
#endif
}

inline bool windowIsFadingOut(const PHLWINDOW& window) {
#if HYM_HYPRLAND_0_56
    // 0.56 detaches window fadeouts into immutable snapshots, so a live window never owns that state.
    return false;
#else
    return window && window->m_fadingOut;
#endif
}

inline bool layerIsReadyToDelete(const PHLLS& layer) {
#if HYM_HYPRLAND_0_56
    // 0.56 removes unmapped layers from LayerState before rendering their detached fadeout snapshot.
    return false;
#else
    return layer && layer->m_readyToDelete;
#endif
}

inline bool windowIsEffectiveFullscreen(const PHLWINDOW& window, FullscreenMode mode) {
#if HYM_HYPRLAND_0_56
    return window && Fullscreen::controller()->isFullscreen(window, mode);
#else
    return window && window->isEffectiveInternalFSMode(mode);
#endif
}

inline bool workspaceHasFullscreen(const PHLWORKSPACE& workspace) {
#if HYM_HYPRLAND_0_56
    return workspace && Fullscreen::controller()->hasFullscreen(workspace, std::nullopt);
#else
    return workspace && workspace->m_hasFullscreenWindow;
#endif
}

inline FullscreenMode workspaceFullscreenMode(const PHLWORKSPACE& workspace) {
#if HYM_HYPRLAND_0_56
    return workspace ? Fullscreen::controller()->getFullscreenModes(workspace, std::nullopt).internal : FULLSCREEN_NONE;
#else
    return workspace && workspace->m_hasFullscreenWindow ? workspace->m_fullscreenMode : FULLSCREEN_NONE;
#endif
}

inline PHLWINDOW workspaceFullscreenWindow(const PHLWORKSPACE& workspace) {
#if HYM_HYPRLAND_0_56
    return workspace ? Fullscreen::controller()->getFullscreenWindow(workspace, std::nullopt) : PHLWINDOW{};
#else
    return workspace ? workspace->getFullscreenWindow() : PHLWINDOW{};
#endif
}

inline FullscreenMode windowFullscreenMode(const PHLWINDOW& window) {
#if HYM_HYPRLAND_0_56
    return window ? Fullscreen::controller()->getFullscreenModes(window).internal : FULLSCREEN_NONE;
#else
    return window ? window->m_fullscreenState.internal : FULLSCREEN_NONE;
#endif
}

inline void setWorkspaceFullscreenRenderState(const PHLWORKSPACE& workspace, bool hasFullscreen, FullscreenMode mode) {
#if HYM_HYPRLAND_0_56
    (void)mode;
    if (!workspace)
        return;

    for (const auto& window : windows()) {
        if (!window || window->m_workspace != workspace)
            continue;

        auto& fullscreenAlpha = window->alpha(Desktop::View::WINDOW_ALPHA_FULLSCREEN);
        if (!hasFullscreen)
            fullscreenAlpha->setValueAndWarp(1.0F);
        else
            *fullscreenAlpha = window->isBlockedByFullscreen() ? 0.0F : 1.0F;
    }
#else
    if (!workspace)
        return;

    workspace->m_hasFullscreenWindow = hasFullscreen;
    workspace->m_fullscreenMode = mode;
#endif
}

inline void setResizeCursorOverride(const std::string& name) {
#if HYM_HYPRLAND_0_56
    Pointer::Cursor::overrideController->setOverride(name, Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
#else
    Cursor::overrideController->setOverride(name, Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
#endif
}

inline void clearResizeCursorOverride() {
#if HYM_HYPRLAND_0_56
    Pointer::Cursor::overrideController->unsetOverride(Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
#else
    Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
#endif
}

} // namespace hymission::hyprland_compat

namespace hymission {
using eFullscreenMode = hyprland_compat::FullscreenMode;
inline constexpr eFullscreenMode FSMODE_NONE = hyprland_compat::FULLSCREEN_NONE;
inline constexpr eFullscreenMode FSMODE_FULLSCREEN = hyprland_compat::FULLSCREEN_FULL;
}
