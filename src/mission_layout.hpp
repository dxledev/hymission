#pragma once

// Pure, compositor-independent overview layout API.
//
// Callers provide each window's natural rectangle and a monitor-local content
// area. MissionControlLayout returns scaled target slots without modifying any
// Hyprland window state. The implementation supports a row-based grid solver
// and a position-preserving natural solver.

#include <cstddef>
#include <string>
#include <vector>

namespace hymission {

// Rectangle used by all pure geometry modules. Controller code converts
// between this type and Hyprland's CBox/Vector2D types at subsystem boundaries.
struct Rect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    [[nodiscard]] double centerX() const {
        return x + width / 2.0;
    }

    [[nodiscard]] double centerY() const {
        return y + height / 2.0;
    }
};

// Stable input record for one window. `index` is carried through the solver so
// a caller can restore its own ordering after the solver reorders candidates.
struct WindowInput {
    std::size_t index = 0;
    Rect        natural;
    std::string label;
    std::size_t rowGroup = 0;
    double      layoutEmphasis = 1.0;
};

// Final overview placement for one input window.
struct WindowSlot {
    std::size_t index = 0;
    Rect        natural;
    Rect        target;
    double      scale = 1.0;
};

// Grid favors consistently sized rows; Natural favors original spatial order.
enum class LayoutEngine {
    Grid,
    Natural,
};

// User-tunable constraints shared by both layout engines.
struct LayoutConfig {
    LayoutEngine engine = LayoutEngine::Grid;
    double outerPaddingTop = 48.0;
    double outerPaddingRight = 48.0;
    double outerPaddingBottom = 48.0;
    double outerPaddingLeft = 48.0;
    double rowSpacing = 32.0;
    double columnSpacing = 32.0;
    double smallWindowBoost = 1.35;
    double maxPreviewScale = 0.95;
    double minWindowLength = 120.0;
    double minPreviewShortEdge = 32.0;
    double layoutSpaceWeight = 0.10;
    double layoutScaleWeight = 1.0;
    double minSlotScale = 0.10;
    double naturalScaleFlex = 0.22;
    bool   preserveInputOrder = false;
    bool   forceRowGroups = false;
    bool   rankScaleByInputOrder = false;
};

// Stateless entry point for normal (non-direct-niri) overview placement.
class MissionControlLayout {
  public:
    [[nodiscard]] std::vector<WindowSlot> compute(const std::vector<WindowInput>& windows, const Rect& area, const LayoutConfig& config = {}) const;
};

} // namespace hymission
