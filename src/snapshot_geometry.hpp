#pragma once

#include "mission_layout.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace hymission::snapshot_geometry {

struct PixelCopy {
    Rect source;
    Rect destination;
};

inline Rect roundSurfaceRect(const Rect& rect) {
    const double left = std::round(rect.x);
    const double top = std::round(rect.y);
    return {left, top, std::round(rect.x + rect.width) - left, std::round(rect.y + rect.height) - top};
}

inline Rect intersectRects(const Rect& first, const Rect& second) {
    const double left = std::max(first.x, second.x);
    const double top = std::max(first.y, second.y);
    return {left, top, std::max(0.0, std::min(first.x + first.width, second.x + second.width) - left),
            std::max(0.0, std::min(first.y + first.height, second.y + second.height) - top)};
}

inline std::optional<PixelCopy> clippedPixelCopy(const Rect& sourceRect, double destinationX, double destinationY,
                                                double sourceWidth, double sourceHeight, double destinationWidth, double destinationHeight) {
    const Rect roundedSource = roundSurfaceRect(sourceRect);
    const Rect clippedSource = intersectRects(roundedSource, {0.0, 0.0, sourceWidth, sourceHeight});
    const Rect destination{std::round(destinationX) + clippedSource.x - roundedSource.x,
                           std::round(destinationY) + clippedSource.y - roundedSource.y, clippedSource.width, clippedSource.height};
    const Rect clippedDestination = intersectRects(destination, {0.0, 0.0, destinationWidth, destinationHeight});
    if (clippedDestination.width <= 0.0 || clippedDestination.height <= 0.0)
        return std::nullopt;
    return PixelCopy{{clippedSource.x + clippedDestination.x - destination.x, clippedSource.y + clippedDestination.y - destination.y,
                      clippedDestination.width, clippedDestination.height}, clippedDestination};
}

inline std::optional<Rect> contentTexelUVRect(const Rect& content, double framebufferWidth, double framebufferHeight) {
    if (framebufferWidth <= 0.0 || framebufferHeight <= 0.0)
        return std::nullopt;
    const Rect clipped = intersectRects(content, {0.0, 0.0, framebufferWidth, framebufferHeight});
    if (clipped.width < 1.0 || clipped.height < 1.0)
        return std::nullopt;
    return Rect{(clipped.x + 0.5) / framebufferWidth, (clipped.y + 0.5) / framebufferHeight,
                (clipped.width - 1.0) / framebufferWidth, (clipped.height - 1.0) / framebufferHeight};
}

}
