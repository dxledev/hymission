#include "snapshot_geometry.hpp"

#include <cmath>
#include <iostream>

using hymission::Rect;
using namespace hymission::snapshot_geometry;

namespace {

bool expect(bool condition, const char* message) {
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool sameRect(const Rect& actual, const Rect& expected) {
    return std::abs(actual.x - expected.x) < 1e-9 && std::abs(actual.y - expected.y) < 1e-9 &&
        std::abs(actual.width - expected.width) < 1e-9 && std::abs(actual.height - expected.height) < 1e-9;
}

bool sameBlitRect(const FramebufferBlitRect& actual, const FramebufferBlitRect& expected) {
    return actual.left == expected.left && actual.bottom == expected.bottom && actual.right == expected.right && actual.top == expected.top;
}

bool checkStrip(const Rect& source, const Rect& expectedSource, double padding, double framebufferWidth, double framebufferHeight) {
    const double proxyWidth = std::ceil(source.width + 2.0 * padding);
    const double proxyHeight = std::ceil(source.height + 2.0 * padding);
    const auto copy = clippedPixelCopy(source, padding, padding, framebufferWidth, framebufferHeight, proxyWidth, proxyHeight);
    if (!expect(copy.has_value(), "edge strip must produce a copy"))
        return false;
    bool ok = expect(sameRect(copy->source, expectedSource), "edge strip must use rounded surface endpoints");
    ok &= expect(sameRect(copy->destination, {padding, padding, expectedSource.width, expectedSource.height}),
                 "edge strip must copy every source texel without resizing");
    const auto sourceBlit = rectToFramebufferBlitRect(copy->source, framebufferWidth, framebufferHeight, FramebufferYConvention::Direct);
    const auto destinationBlit = rectToFramebufferBlitRect(copy->destination, proxyWidth, proxyHeight, FramebufferYConvention::Direct);
    ok &= expect(sourceBlit && sameBlitRect(*sourceBlit, {static_cast<int>(expectedSource.x), static_cast<int>(expectedSource.y),
                                                        static_cast<int>(expectedSource.x + expectedSource.width),
                                                        static_cast<int>(expectedSource.y + expectedSource.height)}),
                 "snapshot source GL coordinates must use direct Y");
    ok &= expect(destinationBlit && sameBlitRect(*destinationBlit, {static_cast<int>(padding), static_cast<int>(padding),
                                                                  static_cast<int>(padding + expectedSource.width),
                                                                  static_cast<int>(padding + expectedSource.height)}),
                 "snapshot destination GL coordinates must use direct Y with padding");
    const auto uv = contentTexelUVRect(copy->destination, proxyWidth, proxyHeight);
    if (!expect(uv.has_value(), "edge strip must produce content UVs"))
        return false;
    ok &= expect(std::abs(uv->x * proxyWidth - (padding + 0.5)) < 1e-9 &&
                 std::abs((uv->x + uv->width) * proxyWidth - (padding + expectedSource.width - 0.5)) < 1e-9,
                 "horizontal UVs must span first and last copied texel centers");
    ok &= expect(std::abs(uv->y * proxyHeight - (padding + 0.5)) < 1e-9 &&
                 std::abs((uv->y + uv->height) * proxyHeight - (padding + expectedSource.height - 0.5)) < 1e-9,
                 "vertical UVs must span first and last copied texel centers");
    return ok;
}

}

int main() {
    bool ok = true;
    for (const double scale : {1.0, 1.25}) {
        const double padding = 24.0 * scale;
        const double thickness = 10.0 * scale;
        const double leadingThickness = scale == 1.0 ? 10.0 : 13.0;
        const double trailingThickness = scale == 1.0 ? 10.0 : 12.0;
        const double barHeight = 65.0 * scale;
        const double roundedBarHeight = scale == 1.0 ? 65.0 : 81.0;
        ok &= checkStrip({0.0, 0.0, 2560.0, barHeight}, {0.0, 0.0, 2560.0, roundedBarHeight}, padding, 2560.0, 1440.0);
        for (const double cornerX : {0.0, 2560.0 - thickness}) {
            for (const double cornerY : {0.0, 1440.0 - thickness}) {
                const double cornerWidth = cornerX == 0.0 ? leadingThickness : trailingThickness;
                const double cornerHeight = cornerY == 0.0 ? leadingThickness : trailingThickness;
                ok &= checkStrip({cornerX, cornerY, thickness, thickness},
                                 {cornerX == 0.0 ? 0.0 : 2560.0 - cornerWidth, cornerY == 0.0 ? 0.0 : 1440.0 - cornerHeight,
                                  cornerWidth, cornerHeight}, padding, 2560.0, 1440.0);
            }
        }
        ok &= checkStrip({0.0, 0.0, thickness, 1440.0}, {0.0, 0.0, leadingThickness, 1440.0}, padding, 2560.0, 1440.0);
        ok &= checkStrip({2560.0 - thickness, 0.0, thickness, 1440.0},
                         {2560.0 - trailingThickness, 0.0, trailingThickness, 1440.0}, padding, 2560.0, 1440.0);
        ok &= checkStrip({0.0, 1440.0 - thickness, 2560.0, thickness},
                         {0.0, 1440.0 - trailingThickness, 2560.0, trailingThickness}, padding, 2560.0, 1440.0);
    }

    const auto topBarSource = rectToFramebufferBlitRect({0.0, 0.0, 2560.0, 81.0}, 2560.0, 1440.0, FramebufferYConvention::Direct);
    const auto topBarDestination = rectToFramebufferBlitRect({30.0, 30.0, 2560.0, 81.0}, 2620.0, 142.0, FramebufferYConvention::Direct);
    ok &= expect(topBarSource && sameBlitRect(*topBarSource, {0, 0, 2560, 81}), "top bar source GL rows must be 0 through 81");
    ok &= expect(topBarDestination && sameBlitRect(*topBarDestination, {30, 30, 2590, 111}),
                 "top bar destination GL rows must be 30 through 111");
    const auto flippedSource = rectToFramebufferBlitRect({0.0, 0.0, 2560.0, 81.0}, 2560.0, 1440.0);
    const auto flippedDestination = rectToFramebufferBlitRect({30.0, 30.0, 2560.0, 81.0}, 2620.0, 142.0);
    ok &= expect(flippedSource && sameBlitRect(*flippedSource, {0, 1359, 2560, 1440}) &&
                 flippedDestination && sameBlitRect(*flippedDestination, {30, 31, 2590, 112}),
                 "other blit callers must retain the flipped Y default");
    for (const auto convention : {FramebufferYConvention::Direct, FramebufferYConvention::Flipped}) {
        const auto clippedBlit = rectToFramebufferBlitRect({-2.5, -1.5, 15.0, 8.0}, 10.0, 10.0, convention);
        const FramebufferBlitRect expected = convention == FramebufferYConvention::Direct ? FramebufferBlitRect{0, 0, 10, 7} : FramebufferBlitRect{0, 3, 10, 10};
        ok &= expect(clippedBlit && sameBlitRect(*clippedBlit, expected), "GL coordinates must clamp rounded endpoints to framebuffer bounds");
        ok &= expect(!rectToFramebufferBlitRect({10.0, 0.0, 1.0, 1.0}, 10.0, 10.0, convention),
                     "empty GL blits must be rejected in both conventions");
    }

    const auto clipped = clippedPixelCopy({-2.5, -1.5, 15.0, 8.0}, 30.0, 30.0, 2560.0, 1440.0, 76.0, 68.0);
    ok &= expect(clipped && sameRect(clipped->source, {0.0, 0.0, 13.0, 7.0}) &&
                 sameRect(clipped->destination, {33.0, 32.0, 13.0, 7.0}), "source clipping must preserve the destination offset");
    const auto overflow = clippedPixelCopy({2550.0, 1435.0, 20.0, 10.0}, 30.0, 30.0, 2560.0, 1440.0, 80.0, 70.0);
    ok &= expect(overflow && sameRect(overflow->source, {2550.0, 1435.0, 10.0, 5.0}) &&
                 sameRect(overflow->destination, {30.0, 30.0, 10.0, 5.0}), "right and bottom overflow must clip to framebuffer bounds");
    const auto targetClipped = clippedPixelCopy({20.0, 40.0, 13.0, 8.0}, -2.0, -1.0, 2560.0, 1440.0, 9.0, 5.0);
    ok &= expect(targetClipped && sameRect(targetClipped->source, {22.0, 41.0, 9.0, 5.0}) &&
                 sameRect(targetClipped->destination, {0.0, 0.0, 9.0, 5.0}), "destination clipping must preserve one-to-one source texels");
    const auto captured = clippedPixelCopy({0.0, 0.0, 13.0, 1440.0}, 30.0, 30.0, 13.0, 1440.0, 73.0, 1500.0);
    ok &= expect(captured && sameRect(captured->destination, {30.0, 30.0, 13.0, 1440.0}),
                 "captured-sized snapshots must record their actual copied size");
    const auto padded = clippedPixelCopy({0.0, 0.0, 73.0, 1500.0}, 0.0, 0.0, 73.0, 1500.0, 73.0, 1500.0);
    ok &= expect(padded && sameRect(intersectRects(roundSurfaceRect({30.0, 30.0, 12.5, 1440.0}), padded->destination),
                                   {30.0, 30.0, 13.0, 1440.0}), "proxy-sized snapshots must exclude their blur padding from content");
    const auto singleTexel = contentTexelUVRect({3.0, 4.0, 1.0, 1.0}, 10.0, 10.0);
    ok &= expect(singleTexel && sameRect(*singleTexel, {0.35, 0.45, 0.0, 0.0}), "single-texel content must sample its center");
    ok &= expect(!clippedPixelCopy({2560.0, 0.0, 10.0, 10.0}, 30.0, 30.0, 2560.0, 1440.0, 70.0, 70.0),
                 "fully clipped captures must be rejected");
    ok &= expect(!contentTexelUVRect({}, 70.0, 70.0) && !contentTexelUVRect({0.0, 0.0, 10.0, 10.0}, 0.0, 70.0),
                 "missing content and invalid framebuffers must not produce UVs");
    return ok ? 0 : 1;
}
