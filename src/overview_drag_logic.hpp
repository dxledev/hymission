#pragma once

// Pure geometry used by direct-niri window dragging.
//
// This module has no Hyprland dependency: the controller projects live
// scrolling columns into these records, asks for an insertion target or edge
// velocity, and then applies the chosen result through Hyprland.

#include <cstddef>
#include <optional>
#include <vector>

namespace hymission::overview_drag {

// Primary direction in which scrolling columns are arranged.
enum class Axis {
    Horizontal,
    Vertical,
};

// A projected tile and its parent column in overview-global coordinates.
struct Rect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct Tile {
    std::size_t index = 0;
    Rect rect;
};

struct Column {
    std::size_t index = 0;
    Rect rect;
    std::vector<Tile> tiles;
};

// A drop either creates a column or inserts a tile into an existing column.
enum class InsertKind {
    NewColumn,
    InColumn,
};

// Logical drop destination plus the thin rectangle rendered as its visual hint.
struct InsertTarget {
    InsertKind kind = InsertKind::NewColumn;
    std::size_t column = 0;
    std::size_t tile = 0;
    Rect hint;
};

// Resolves the pointer to the nearest valid scrolling-layout insertion point.
[[nodiscard]] std::optional<InsertTarget> insertionTarget(const Rect &workspace, const std::vector<Column> &columns, double pointerX, double pointerY,
                                                          Axis primaryAxis, bool reversed, const Rect &draggedPreview);
// Returns signed pixels per second near a viewport edge, or zero outside it.
[[nodiscard]] double edgeScrollVelocity(const Rect &workspace, double pointerX, double pointerY, Axis primaryAxis, bool reversed, double triggerWidth,
                                        double maxSpeed);

} // namespace hymission::overview_drag
