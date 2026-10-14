// Regression tests for overview input policy, gestures, strip geometry, and
// direct-niri projection helpers.

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

#include "overview_logic.hpp"

namespace {

using hymission::Direction;
using hymission::GestureAxis;
using hymission::HymissionScrollMode;
using hymission::Rect;
using hymission::RecommandVisibleGestureMode;
using hymission::ScrollingLayoutDirection;
using hymission::WorkspaceStripAnchor;
using hymission::WorkspaceStripEmptyMode;
using hymission::WorkspaceStripReservation;
using hymission::resolveNativeLayoutTransitionGoal;
using hymission::workspaceTransitionWindowRect;

bool expect(bool condition, const char* message) {
    if (condition)
        return true;

    std::cerr << "FAIL: " << message << '\n';
    return false;
}

bool closeEnough(double actual, double expected, double epsilon = 1e-9) {
    return std::abs(actual - expected) <= epsilon;
}

bool expectRect(const Rect& actual, const Rect& expected, const char* message) {
    return expect(closeEnough(actual.x, expected.x) && closeEnough(actual.y, expected.y) && closeEnough(actual.width, expected.width) &&
                      closeEnough(actual.height, expected.height),
                  message);
}

bool expectOptionalRect(const std::optional<Rect>& actual, const Rect& expected, const char* message) {
    if (!actual)
        return expect(false, message);
    return expectRect(*actual, expected, message);
}

bool expectReservation(const WorkspaceStripReservation& actual, const WorkspaceStripReservation& expected, const char* message) {
    return expectRect(actual.band, expected.band, message) && expectRect(actual.content, expected.content, message);
}

bool testOverviewRounding() {
    bool ok = true;
    ok &= expect(hymission::scaledOverviewRounding(26.4, 1.0, 0.4) == 10,
                 "overview corners should scale the native radius without a border-only multiplier");
    ok &= expect(hymission::scaledOverviewRounding(26.4, 1.25, 0.4) == 13,
                 "overview corners should include fractional monitor scaling");
    ok &= expect(hymission::scaledOverviewRounding(26.4, 1.25, 1.0) == 33,
                 "unscaled overview corners should match Hyprland's surface radius");
    ok &= expect(hymission::scaledOverviewRounding(26.4, 1.25, 1.2) == 40,
                 "enlarged previews should scale their corners with their surfaces");
    ok &= expect(hymission::scaledOverviewRounding(26.4, 1.25, 0.4 * 0.25) == 3,
                 "strip thumbnails should apply their framebuffer downscale to both corner edges");
    ok &= expect(hymission::scaledOverviewRounding(1.9, 1.0, 0.3) == 0,
                 "tiny corners should preserve Hyprland's integer surface-pass quantization");
    ok &= expect(hymission::scaledOverviewRounding(0.0, 2.0, 0.4) == 0,
                 "square or fullscreen windows should retain square overview corners");
    ok &= expect(hymission::scaledOverviewRounding(110.0, 1.25, 0.4) == 55,
                 "Hyprland's power-adjusted radius should not be corrected a second time");
    ok &= expect(hymission::overviewBorderOuterRounding(13, 2.4F, 2, 1.25) == 15,
                 "outer corners should include the monitor-scaled border width");
    ok &= expect(hymission::overviewBorderOuterRounding(13, 10.0F, 2, 1.25) == 15,
                 "squircle borders should preserve the native rounding power");
    ok &= expect(hymission::overviewBorderOuterRounding(13, 1.0F, 2, 1.25) == 14,
                 "outer corners below power two should use Hyprland's diagonal correction");
    ok &= expect(hymission::overviewBorderOuterRounding(0, 2.4F, 2, 1.25) == 0,
                 "a square window should not acquire a rounded overview border");
    return ok;
}

bool testNamespacePatterns() {
    bool ok = true;
    ok &= expect(hymission::matchesNamespacePattern("awww-daemon", "awww-daemon"), "namespace patterns should preserve exact matches");
    ok &= expect(hymission::matchesNamespacePattern("noctalia-wallpaper", "noctalia-.*"), "namespace patterns should support regex wildcards");
    ok &= expect(hymission::matchesNamespacePattern("noctalia-desktop-widget-clock-0001", "noctalia-desktop-widget-.*"),
                 "namespace patterns should match widget namespace suffixes");
    ok &= expect(!hymission::matchesNamespacePattern("noctalia-wallpaper-extra", "noctalia-.*wallpaper"),
                 "namespace regexes should match the complete namespace");
    ok &= expect(!hymission::matchesNamespacePattern("waybar", "waydbar"), "non-matching namespace regexes should be rejected");
    ok &= expect(!hymission::matchesNamespacePattern("waybar", "*"), "invalid namespace regexes should be ignored");
    ok &= expect(!hymission::matchesNamespacePattern("[", "["), "invalid regex metacharacters should not fall back to literal matching");
    return ok;
}

bool testWorkspaceLaneProjection() {
    bool ok = true;
    const Rect workspaceArea{100, 50, 1600, 900};
    ok &= expectRect(transformLiveOverviewRect({500, 230, 400, 225}, workspaceArea, {300, 250, 640, 360}),
                     {460, 322, 160, 90}, "workspace lane projection should preserve each window's relative position and size");
    ok &= expectRect(transformLiveOverviewRect({500, 230, 400, 225}, workspaceArea, {300, 200, 500, 400}),
                     {425, 315.625, 125, 70.3125}, "workspace lane projection should preserve aspect ratio and center letterboxed content");
    ok &= expectRect(transformLiveOverviewRect({500, 230, 400, 225}, workspaceArea, {300, 600, 500, 400}),
                     {425, 715.625, 125, 70.3125}, "the same native target should follow its workspace lane offset");
    ok &= expectRect(transformLiveOverviewRect({900, 350, 600, 300}, workspaceArea, {300, 250, 640, 360}),
                     {620, 370, 240, 120}, "updated native layout targets should map into the lane after a compositor dispatch");
    ok &= expectRect(transformLiveOverviewRect({100, 50, 1600, 900}, workspaceArea, {300, 220, 320, 180}),
                     {300, 220, 320, 180}, "workspace viewport projection should scale uniformly to the lane dimensions");
    return ok;
}

struct WorkspaceMapScene {
    GestureAxis axis = GestureAxis::Vertical;
    Rect nativeDesktopViewport{1920, 100, 1600, 900};
    std::vector<Rect> laneViewports;
    std::vector<Rect> nativeWindowRects{{2400, 330, 600, 340}, {2440, 340, 520, 300}, {2520, 380, 600, 360}};
    std::vector<Rect> previewWindows;
};

struct WorkspaceMapCameraSources {
    std::vector<Rect> lanes;
    std::vector<Rect> windows;
};

WorkspaceMapScene makeWorkspaceMapScene(GestureAxis axis) {
    WorkspaceMapScene scene;
    scene.axis = axis;
    if (axis == GestureAxis::Vertical)
        scene.laneViewports = {{2300, 180, 400, 225}, {2300, 420, 400, 225}, {2300, 660, 400, 225}};
    else
        scene.laneViewports = {{2060, 300, 400, 225}, {2300, 300, 400, 225}, {2540, 300, 400, 225}};

    for (std::size_t index = 0; index < scene.laneViewports.size(); ++index)
        scene.previewWindows.push_back(transformLiveOverviewRect(scene.nativeWindowRects[index], scene.nativeDesktopViewport, scene.laneViewports[index]));
    return scene;
}

double workspaceMapAxisCenter(const Rect& rect, GestureAxis axis) {
    return axis == GestureAxis::Vertical ? rect.centerY() : rect.centerX();
}

double workspaceMapCrossAxisCenter(const Rect& rect, GestureAxis axis) {
    return axis == GestureAxis::Vertical ? rect.centerX() : rect.centerY();
}

WorkspaceMapCameraSources workspaceMapCameraSources(const WorkspaceMapScene& scene, std::size_t selectedLane) {
    WorkspaceMapCameraSources sources;
    const Rect selectedOverviewViewport = scene.laneViewports[selectedLane];
    for (std::size_t index = 0; index < scene.laneViewports.size(); ++index) {
        sources.lanes.push_back(transformLiveOverviewRect(scene.laneViewports[index], selectedOverviewViewport, scene.nativeDesktopViewport));
        sources.windows.push_back(transformLiveOverviewRect(scene.previewWindows[index], selectedOverviewViewport, scene.nativeDesktopViewport));
    }
    return sources;
}

bool expectWorkspaceMapFrame(const WorkspaceMapScene& scene, const WorkspaceMapCameraSources& sources, double openness) {
    bool ok = true;
    std::vector<Rect> visibleLanes;
    std::vector<Rect> visibleWindows;
    for (std::size_t index = 0; index < scene.laneViewports.size(); ++index) {
        visibleLanes.push_back(lerpRect(sources.lanes[index], scene.laneViewports[index], openness));
        visibleWindows.push_back(lerpRect(sources.windows[index], scene.previewWindows[index], openness));
    }

    constexpr double cameraScale = 4.0;
    const double laneStep = workspaceMapAxisCenter(scene.laneViewports[1], scene.axis) - workspaceMapAxisCenter(scene.laneViewports[0], scene.axis);
    const double expectedSeparation = laneStep * (cameraScale * (1.0 - openness) + openness);
    ok &= expect(closeEnough(workspaceMapAxisCenter(visibleLanes[1], scene.axis) - workspaceMapAxisCenter(visibleLanes[0], scene.axis), expectedSeparation) &&
                     closeEnough(workspaceMapAxisCenter(visibleLanes[2], scene.axis) - workspaceMapAxisCenter(visibleLanes[1], scene.axis), expectedSeparation),
                 "workspace-map zoom should preserve lane order and scale separation at every openness sample");
    ok &= expect(workspaceMapAxisCenter(visibleLanes[0], scene.axis) < workspaceMapAxisCenter(visibleLanes[1], scene.axis) &&
                     workspaceMapAxisCenter(visibleLanes[1], scene.axis) < workspaceMapAxisCenter(visibleLanes[2], scene.axis),
                 "adjacent workspace lanes should not collapse onto the selected lane during zoom");
    ok &= expect(closeEnough(workspaceMapCrossAxisCenter(visibleLanes[0], scene.axis), workspaceMapCrossAxisCenter(visibleLanes[1], scene.axis)) &&
                     closeEnough(workspaceMapCrossAxisCenter(visibleLanes[1], scene.axis), workspaceMapCrossAxisCenter(visibleLanes[2], scene.axis)),
                 "workspace-map zoom should preserve the lane cross-axis position");

    for (std::size_t index = 0; index < scene.laneViewports.size(); ++index) {
        const Rect& lane = visibleLanes[index];
        const Rect& window = visibleWindows[index];
        const Rect& previewLane = scene.laneViewports[index];
        const Rect& previewWindow = scene.previewWindows[index];
        ok &= expect(closeEnough((window.centerX() - lane.x) / lane.width, (previewWindow.centerX() - previewLane.x) / previewLane.width) &&
                         closeEnough((window.centerY() - lane.y) / lane.height, (previewWindow.centerY() - previewLane.y) / previewLane.height) &&
                         closeEnough(window.width / lane.width, previewWindow.width / previewLane.width) &&
                         closeEnough(window.height / lane.height, previewWindow.height / previewLane.height),
                     "each window should remain attached to its own lane throughout workspace-map zoom");
        if (openness == 0.0)
            ok &= expectRect(window, sources.windows[index], "closed windows should use their shared camera-source endpoints");
        if (openness == 1.0)
            ok &= expectRect(window, previewWindow, "open windows should finish at their lane preview rectangles");
    }
    return ok;
}

bool testWorkspaceMapZoomAcrossLanes(GestureAxis axis) {
    constexpr std::size_t activeLane = 1;
    constexpr std::size_t exitSelectedLane = 2;
    constexpr std::size_t reopenedLane = 0;
    const WorkspaceMapScene scene = makeWorkspaceMapScene(axis);
    const WorkspaceMapCameraSources openingSources = workspaceMapCameraSources(scene, activeLane);
    const WorkspaceMapCameraSources closingSources = workspaceMapCameraSources(scene, exitSelectedLane);
    const WorkspaceMapCameraSources reopenedSources = workspaceMapCameraSources(scene, reopenedLane);

    bool ok = true;
    ok &= expectRect(openingSources.lanes[activeLane], scene.nativeDesktopViewport,
                     "the active lane camera endpoint should cover the nonzero-origin desktop viewport");
    ok &= expectRect(openingSources.windows[activeLane], scene.nativeWindowRects[activeLane],
                     "the active lane window should return to its native rectangle at open");
    const double activeLaneOffset = (workspaceMapAxisCenter(scene.laneViewports[0], axis) - workspaceMapAxisCenter(scene.laneViewports[activeLane], axis)) * 4.0;
    const double lastLaneOffset = (workspaceMapAxisCenter(scene.laneViewports[2], axis) - workspaceMapAxisCenter(scene.laneViewports[activeLane], axis)) * 4.0;
    const Rect expectedFirstLane = axis == GestureAxis::Vertical ? Rect{1920, 100 + activeLaneOffset, 1600, 900} :
                                                                  Rect{1920 + activeLaneOffset, 100, 1600, 900};
    const Rect expectedLastLane = axis == GestureAxis::Vertical ? Rect{1920, 100 + lastLaneOffset, 1600, 900} :
                                                                 Rect{1920 + lastLaneOffset, 100, 1600, 900};
    ok &= expectRect(openingSources.lanes[0], expectedFirstLane, "the first lane camera endpoint should retain its offset on the selected axis");
    ok &= expectRect(openingSources.lanes[2], expectedLastLane, "the last lane camera endpoint should retain its offset on the selected axis");
    ok &= expectRect(closingSources.lanes[exitSelectedLane], scene.nativeDesktopViewport,
                     "the newly selected lane should become the native desktop endpoint on close");
    ok &= expectRect(closingSources.windows[exitSelectedLane], scene.nativeWindowRects[exitSelectedLane],
                     "the close path should end at the newly selected lane's native window");
    ok &= expectRect(reopenedSources.lanes[reopenedLane], scene.nativeDesktopViewport,
                     "reopening from another selected lane should use that lane as the native desktop endpoint");
    ok &= expectRect(reopenedSources.windows[reopenedLane], scene.nativeWindowRects[reopenedLane],
                     "reopening from another selected lane should return its own window to native geometry");

    for (const double openness : {0.0, 0.25, 0.5, 0.75, 1.0})
        ok &= expectWorkspaceMapFrame(scene, openingSources, openness);
    for (const double openness : {1.0, 0.75, 0.5, 0.25, 0.0})
        ok &= expectWorkspaceMapFrame(scene, closingSources, openness);
    for (const double openness : {0.0, 0.5, 1.0})
        ok &= expectWorkspaceMapFrame(scene, reopenedSources, openness);
    return ok;
}

bool testNativeLayoutTransitionGoalPolicy(Rect& smallPreview, Rect& largePreview) {
    const Rect desktopViewport{1920, 100, 1920, 1080};
    const Rect sourceLane{2500, 350, 480, 270};
    const Rect smallNativeGoal{2400, 370, 960, 540};
    const Rect retainedLargeGoal{1920, 100, 1920, 1080};
    const auto acceptedGoal = resolveNativeLayoutTransitionGoal(retainedLargeGoal, false, retainedLargeGoal);
    const auto changedGoal = resolveNativeLayoutTransitionGoal(smallNativeGoal, false, retainedLargeGoal);
    const auto projectedLargeGoal = resolveNativeLayoutTransitionGoal(retainedLargeGoal, true, smallNativeGoal);
    const auto projectedSmallGoal = resolveNativeLayoutTransitionGoal(smallNativeGoal, true, retainedLargeGoal);

    bool ok = true;
    ok &= expectOptionalRect(acceptedGoal, retainedLargeGoal,
                 "a retained native goal should remain usable after its compositor animation settles");
    ok &= expect(!changedGoal, "a later native layout goal should invalidate the retained geometry");
    ok &= expect(!resolveNativeLayoutTransitionGoal(retainedLargeGoal, false), "a live goal without a retained target should stay dynamic");

    const auto approximateGoal = resolveNativeLayoutTransitionGoal({1920.5, 99.5, 1920.5, 1079.5}, false, retainedLargeGoal);
    ok &= expect(approximateGoal.has_value(), "retained goals should tolerate compositor rounding up to one half pixel");
    ok &= expect(!resolveNativeLayoutTransitionGoal({1920.51, 100, 1920, 1080}, false, retainedLargeGoal),
                 "retained goals should invalidate when a later native edit changes a field beyond tolerance");
    ok &= expect(!resolveNativeLayoutTransitionGoal(retainedLargeGoal, false,
                                                      Rect{std::numeric_limits<double>::quiet_NaN(), 100, 1920, 1080}),
                 "non-finite retained goals should not match a live native goal");
    ok &= expectOptionalRect(projectedLargeGoal, retainedLargeGoal,
                             "growing transition targets should use the full native goal even when the live goal is half-sized");
    ok &= expectOptionalRect(projectedSmallGoal, smallNativeGoal,
                             "shrinking transition targets should use the new small native goal despite retained full geometry");
    ok &= expect(!resolveNativeLayoutTransitionGoal({std::numeric_limits<double>::quiet_NaN(), 0, 100, 100}, true),
                 "native goals with NaN coordinates should be rejected");
    ok &= expect(!resolveNativeLayoutTransitionGoal({0, std::numeric_limits<double>::infinity(), 100, 100}, true),
                 "native goals with infinite coordinates should be rejected");
    ok &= expect(!resolveNativeLayoutTransitionGoal({0, 0, 1, 100}, true), "native goals with dimensions no larger than one should be rejected");
    ok &= expect(!resolveNativeLayoutTransitionGoal({0, 0, 100, 0}, true), "native goals with nonpositive dimensions should be rejected");

    smallPreview = transformLiveOverviewRect(projectedSmallGoal.value_or(Rect{}), desktopViewport, sourceLane);
    largePreview = transformLiveOverviewRect(projectedLargeGoal.value_or(Rect{}), desktopViewport, sourceLane);
    ok &= expect(closeEnough(smallPreview.width * 2.0, largePreview.width) && closeEnough(smallPreview.height * 2.0, largePreview.height),
                 "projected small and full goals should differ in both width and height");
    ok &= expectRect(largePreview, sourceLane, "the projected full target should fill its preview lane");
    ok &= expect(closeEnough(smallPreview.width, sourceLane.width * 0.5) && closeEnough(smallPreview.height, sourceLane.height * 0.5),
                 "the projected half-size target should preserve its smaller width and height");
    return ok;
}

Rect centeredPreviewRect(const Rect& lane, double width, double height) {
    return {lane.centerX() - width * 0.5, lane.centerY() - height * 0.5, width, height};
}

bool testWorkspaceTransitionWindowSizeSamples(const Rect& smallPreview, const Rect& largePreview, GestureAxis axis,
                                             bool widthGrows, bool heightGrows, bool reverse) {
    constexpr double distance = 900.0;
    const Rect firstLane{2500, 350, 480, 270};
    const Rect adjacentLane = axis == GestureAxis::Horizontal ? Rect{2740, 350, 480, 270} : Rect{2500, 590, 480, 270};
    const Rect sourceLane = reverse ? adjacentLane : firstLane;
    const Rect targetLane = reverse ? firstLane : adjacentLane;
    const double smallWidth = std::min(smallPreview.width, largePreview.width);
    const double largeWidth = std::max(smallPreview.width, largePreview.width);
    const double smallHeight = std::min(smallPreview.height, largePreview.height);
    const double largeHeight = std::max(smallPreview.height, largePreview.height);
    const double sourceWidth = widthGrows ? smallWidth : largeWidth;
    const double targetWidth = widthGrows ? largeWidth : smallWidth;
    const double sourceHeight = heightGrows ? smallHeight : largeHeight;
    const double targetHeight = heightGrows ? largeHeight : smallHeight;
    const Rect source = centeredPreviewRect(sourceLane, sourceWidth, sourceHeight);
    const Rect target = centeredPreviewRect(targetLane, targetWidth, targetHeight);
    const int step = reverse ? -1 : 1;
    const double midpointDelta = reverse ? -450.0 : 450.0;
    const double nearEndDelta = reverse ? -899.0 : 899.0;
    const double endDelta = reverse ? -distance : distance;

    const auto start = workspaceTransitionWindowRect(source, target, axis, distance, 0.0, step);
    const auto middle = workspaceTransitionWindowRect(source, target, axis, distance, midpointDelta, step);
    const auto nearEnd = workspaceTransitionWindowRect(source, target, axis, distance, nearEndDelta, step);
    const auto end = workspaceTransitionWindowRect(source, target, axis, distance, endDelta, step);
    bool ok = true;
    ok &= expectOptionalRect(start, source, "transferred windows should start from the captured source preview");
    ok &= expect(middle.has_value() && closeEnough(middle->width, (sourceWidth + targetWidth) * 0.5) &&
                     closeEnough(middle->height, (sourceHeight + targetHeight) * 0.5),
                 "transferred windows should interpolate width and height independently at the midpoint");
    if (middle) {
        ok &= expect(closeEnough(workspaceMapAxisCenter(*middle, axis),
                                 (workspaceMapAxisCenter(source, axis) + workspaceMapAxisCenter(target, axis)) * 0.5) &&
                         closeEnough(workspaceMapCrossAxisCenter(*middle, axis),
                                     (workspaceMapCrossAxisCenter(source, axis) + workspaceMapCrossAxisCenter(target, axis)) * 0.5),
                     "transferred windows should follow the selected lane axis while preserving cross-axis interpolation");
    }
    ok &= expect(nearEnd.has_value() && std::abs(nearEnd->width - targetWidth) < 0.5 && std::abs(nearEnd->height - targetHeight) < 0.5,
                 "transferred window size should approach both target dimensions before commit");
    ok &= expectOptionalRect(end, target, "workspace transition commit should land on the projected target preview");
    if (nearEnd && end)
        ok &= expect(std::abs(nearEnd->x - end->x) < 2.0 && std::abs(nearEnd->y - end->y) < 2.0 &&
                         std::abs(nearEnd->width - end->width) < 0.5 && std::abs(nearEnd->height - end->height) < 0.5,
                     "the final transition frame should stay continuous with the committed preview");
    return ok;
}

bool testWorkspaceTransitionWindowFallbacks(const Rect& smallPreview, const Rect& largePreview) {
    constexpr double distance = 900.0;
    bool ok = true;
    for (const GestureAxis axis : {GestureAxis::Horizontal, GestureAxis::Vertical}) {
        const auto pinnedMidpoint = workspaceTransitionWindowRect(smallPreview, largePreview, axis, distance, 450.0, 1, true);
        ok &= expectOptionalRect(pinnedMidpoint, lerpRect(smallPreview, largePreview, 0.5),
                                 "pinned windows should interpolate geometry without workspace offsets");
        ok &= expectOptionalRect(workspaceTransitionWindowRect(smallPreview, std::nullopt, axis, distance, 450.0, 1, true), smallPreview,
                                 "one-sided pinned windows should remain at their captured geometry");
        const auto oneSidedSource = workspaceTransitionWindowRect(smallPreview, std::nullopt, axis, distance, 450.0, 1);
        const auto oneSidedTarget = workspaceTransitionWindowRect(std::nullopt, largePreview, axis, distance, 450.0, 1);
        const Rect expectedSource = axis == GestureAxis::Horizontal ? Rect{smallPreview.x - 450.0, smallPreview.y, smallPreview.width, smallPreview.height} :
                                                                      Rect{smallPreview.x, smallPreview.y - 450.0, smallPreview.width, smallPreview.height};
        const Rect expectedTarget = axis == GestureAxis::Horizontal ? Rect{largePreview.x + 450.0, largePreview.y, largePreview.width, largePreview.height} :
                                                                      Rect{largePreview.x, largePreview.y + 450.0, largePreview.width, largePreview.height};
        ok &= expectOptionalRect(oneSidedSource, expectedSource, "source-only transition windows should preserve their prior translation behavior");
        ok &= expectOptionalRect(oneSidedTarget, expectedTarget, "target-only transition windows should preserve their prior translation behavior");
        ok &= expectOptionalRect(workspaceTransitionWindowRect(smallPreview, largePreview, axis, 0.0, 0.0, 1), largePreview,
                                 "zero-distance transitions should resolve directly to the target endpoint");
        ok &= expectOptionalRect(workspaceTransitionWindowRect(smallPreview, std::nullopt, axis, 0.0, 0.0, 1), smallPreview,
                                 "zero-distance one-sided transitions should preserve their available endpoint");
    }

    ok &= expect(!workspaceTransitionWindowRect(std::nullopt, std::nullopt, GestureAxis::Horizontal, distance, 0.0, 1),
                 "windows absent from both transition states should not produce a transition rect");
    return ok;
}

bool testLoggedWindowTransferFixture() {
    const Rect liveNative{2441.5, 55, 1005, 1085};
    const Rect finalGoal{1932, 55, 2024, 1085};
    const Rect desktopViewport{1920, 10, 2048, 1152};
    const Rect targetViewport{2649.088, 412.992, 589.824, 331.776};
    const auto forcedGoal = resolveNativeLayoutTransitionGoal(finalGoal, true, liveNative);
    const Rect stalePreview = transformLiveOverviewRect(liveNative, desktopViewport, targetViewport);
    const Rect finalPreview = transformLiveOverviewRect(forcedGoal.value_or(Rect{}), desktopViewport, targetViewport);

    bool ok = true;
    ok &= expect(closeEnough(stalePreview.width, 289.44) && closeEnough(finalPreview.width, 582.912),
                 "the logged target should project live width 289.44 and final width 582.912");
    ok &= expect(closeEnough(stalePreview.height, 312.48) && closeEnough(finalPreview.height, 312.48),
                 "the logged transfer should preserve its 312.48 preview height");
    for (const GestureAxis axis : {GestureAxis::Horizontal, GestureAxis::Vertical}) {
        for (const int direction : {-1, 1}) {
            Rect target = finalPreview;
            if (axis == GestureAxis::Horizontal)
                target.x += direction * 240.0;
            else
                target.y += direction * 240.0;
            const auto midpoint = workspaceTransitionWindowRect(stalePreview, target, axis, 900.0, direction * 450.0, direction);
            const auto endpoint = workspaceTransitionWindowRect(stalePreview, target, axis, 900.0, direction * 900.0, direction);
            ok &= expect(midpoint && closeEnough(midpoint->width, 436.176) && closeEnough(midpoint->height, 312.48),
                         "the exact logged fixture should animate width at midpoint while height stays fixed");
            ok &= expectOptionalRect(endpoint, target, "the exact logged fixture should commit at its forced full-width goal");
        }
    }
    return ok;
}

bool testNativeLayoutTransitionGeometry() {
    Rect smallPreview;
    Rect largePreview;
    bool ok = testNativeLayoutTransitionGoalPolicy(smallPreview, largePreview);
    for (const GestureAxis axis : {GestureAxis::Horizontal, GestureAxis::Vertical}) {
        for (const bool widthGrows : {false, true}) {
            for (const bool heightGrows : {false, true}) {
                for (const bool reverse : {false, true})
                    ok &= testWorkspaceTransitionWindowSizeSamples(smallPreview, largePreview, axis, widthGrows, heightGrows, reverse);
            }
        }
    }
    ok &= testWorkspaceTransitionWindowFallbacks(smallPreview, largePreview);
    ok &= testLoggedWindowTransferFixture();
    return ok;
}

} // namespace

int main() {
    using namespace hymission;

    const std::vector<Rect> rects = {
        {0, 0, 100, 100},
        {140, 0, 100, 100},
        {0, 140, 100, 100},
        {140, 140, 100, 100},
    };

    bool ok = testOverviewRounding();
    ok &= testNamespacePatterns();
    ok &= testWorkspaceLaneProjection();
    ok &= testWorkspaceMapZoomAcrossLanes(GestureAxis::Vertical);
    ok &= testWorkspaceMapZoomAcrossLanes(GestureAxis::Horizontal);
    ok &= testNativeLayoutTransitionGeometry();

    ok &= expect(hitTest(rects, 50, 50) == std::optional<std::size_t>{0}, "hitTest should find top-left rect");
    ok &= expect(hitTest(rects, 180, 180) == std::optional<std::size_t>{3}, "hitTest should find bottom-right rect");
    ok &= expect(!hitTest(rects, 120, 120).has_value(), "hitTest should miss gap");

    ok &= expect(chooseDirectionalNeighbor(rects, 0, Direction::Right) == std::optional<std::size_t>{1}, "right neighbor from 0 should be 1");
    ok &= expect(chooseDirectionalNeighbor(rects, 0, Direction::Down) == std::optional<std::size_t>{2}, "down neighbor from 0 should be 2");
    ok &= expect(chooseDirectionalNeighbor(rects, 3, Direction::Left) == std::optional<std::size_t>{2}, "left neighbor from 3 should be 2");
    ok &= expect(!chooseDirectionalNeighbor(rects, 0, Direction::Up).has_value(), "up neighbor from top row should be none");
    ok &= expect(!chooseCyclicIndex(0, 0).has_value(), "cyclic selection should be empty for zero windows");
    ok &= expect(!chooseCyclicIndex(1, 0).has_value(), "cyclic selection should be empty for one window");
    ok &= expect(chooseCyclicIndex(4, 0) == std::optional<std::size_t>{1}, "cyclic selection should advance to the next window");
    ok &= expect(chooseCyclicIndex(4, 3) == std::optional<std::size_t>{0}, "cyclic selection should wrap from the end to the start");
    ok &= expect(chooseCyclicIndex(4, 0, -1) == std::optional<std::size_t>{3}, "cyclic selection should support reverse wrapping");

    const Rect middle = lerpRect({0, 0, 100, 100}, {100, 80, 50, 60}, 0.5);
    ok &= expect(middle.x == 50.0 && middle.y == 40.0 && middle.width == 75.0 && middle.height == 80.0, "lerpRect midpoint should be correct");
    ok &= expectRect(transformLiveOverviewRect({400, 200, 800, 600}, {0, 0, 2000, 1000}, {100, 50, 1000, 500}),
                     {300, 150, 400, 300}, "live overview transform should scale current Hyprland geometry into the overview viewport");
    ok &= expectRect(transformLiveOverviewRect({600, 200, 1000, 600}, {0, 0, 2000, 1000}, {100, 50, 1000, 500}),
                     {400, 150, 500, 300}, "metadata refresh should not freeze later live position and size changes");
    ok &= expectRect(transformLiveOverviewRect({1932, 55, 2024, 1085}, {1920, 10, 2048, 1152}, {710, 392, 624, 351}),
                     {713.65625, 405.7109375, 616.6875, 330.5859375},
                     "dock-hidden window should map into the fixed monitor viewport");
    ok &= expectRect(transformLiveOverviewRect({1932, 55, 2024, 1012}, {1920, 10, 2048, 1152}, {710, 392, 624, 351}),
                     {713.65625, 405.7109375, 616.6875, 308.34375},
                     "dock-visible window should keep its top edge while freeing dock space");
    ok &= expect(authoritativeOverviewWorkspaceId(true, 7, 3) == 7,
                 "active transition destination should remain authoritative during close");
    ok &= expect(authoritativeOverviewWorkspaceId(false, 7, 3) == 3,
                 "committed workspace should be authoritative after transition completion");

    ok &= expect(easeOutCubic(0.0) == 0.0, "easeOutCubic(0) should be 0");
    ok &= expect(easeInCubic(1.0) == 1.0, "easeInCubic(1) should be 1");
    ok &= expect(shouldSyncOverviewLiveFocus(true, true, 1), "live focus should sync when overview and Hyprland follow-mouse are enabled");
    ok &= expect(!shouldSyncOverviewLiveFocus(true, true, 0), "live focus should not sync when Hyprland follow-mouse was disabled before opening overview");
    ok &= expect(!shouldSyncOverviewLiveFocus(false, true, 1), "live focus should not sync when overview input handling is inactive");

    ok &= expect(resolveRecommandVisibleGestureMode(1, -1) == RecommandVisibleGestureMode::TransferCapable,
                 "visible forceall swipes toward compact should allow recommand transfer");
    ok &= expect(resolveRecommandVisibleGestureMode(-1, 1) == RecommandVisibleGestureMode::TransferCapable,
                 "visible compact swipes toward forceall should allow recommand transfer");
    ok &= expect(resolveRecommandVisibleGestureMode(1, 1) == RecommandVisibleGestureMode::CloseOnly,
                 "visible forceall swipes in the other direction should only close");
    ok &= expect(resolveRecommandVisibleGestureMode(-1, -1) == RecommandVisibleGestureMode::CloseOnly,
                 "visible compact swipes in the other direction should only close");

    ok &= expect(resolveOverviewGestureCommit(true, 0.6, 0.0, 30.0, false),
                 "opening gestures past halfway should commit");
    ok &= expect(!resolveOverviewGestureCommit(true, 0.4, -40.0, 30.0, false),
                 "reverse velocity should cancel opening gestures");
    ok &= expect(resolveOverviewGestureCommit(false, 0.4, 40.0, 30.0, false),
                 "close gestures past halfway should commit to hidden");
    ok &= expect(!resolveOverviewGestureCommit(false, 0.6, -40.0, 30.0, false),
                 "reverse velocity should cancel close gestures");

    ok &= expect(resolveRecommandGestureCommitDirection(0.6, true, 0.0, 30.0, false) == 1,
                 "positive recommand gestures past halfway should commit to forceall");
    ok &= expect(resolveRecommandGestureCommitDirection(-0.6, true, 0.0, 30.0, false) == -1,
                 "negative recommand gestures past halfway should commit to compact scope");
    ok &= expect(resolveRecommandGestureCommitDirection(0.8, true, -40.0, 30.0, false) == 0,
                 "reverse velocity should close forceall instead of jumping directly to compact scope");
    ok &= expect(resolveRecommandGestureCommitDirection(-0.8, true, 40.0, 30.0, false) == -1,
                 "opening compact with forward velocity should still commit the current recommand side");
    ok &= expect(resolveRecommandGestureCommitDirection(-0.8, false, 40.0, 30.0, false) == 0,
                 "reverse velocity should close compact scope instead of jumping directly to forceall");
    ok &= expect(resolveRecommandGestureCommitDirection(0.8, false, -40.0, 30.0, false) == 1,
                 "reverse reopening velocity should still be able to keep the current visible recommand side");

    ok &= expect(resolveOverviewWorkspaceChangeAction(true, false, false, false, true, false) == OverviewWorkspaceChangeAction::Rebuild,
                 "live focus workspace changes should rebuild overview instead of aborting");
    ok &= expect(resolveOverviewWorkspaceChangeAction(true, false, false, false, false, false) == OverviewWorkspaceChangeAction::Abort,
                 "external workspace changes without overview transitions should abort multi-workspace overview");
    ok &= expect(resolveOverviewWorkspaceChangeAction(true, false, true, false, false, false) == OverviewWorkspaceChangeAction::Rebuild,
                 "overview workspace transitions should continue rebuilding state");
    ok &= expect(resolveOverviewWorkspaceChangeAction(true, false, false, true, true, true) == OverviewWorkspaceChangeAction::Ignore,
                 "closing overview should ignore workspace changes");
    ok &= expect(resolveOverviewEditTransitionAction(false, true, true, true) == OverviewEditTransitionAction::Run,
                 "editing without an active workspace transition should run immediately");
    ok &= expect(resolveOverviewEditTransitionAction(true, true, true, true) == OverviewEditTransitionAction::Retarget,
                 "timed niri single-workspace transitions should retarget before focus or movement edits");
    ok &= expect(resolveOverviewEditTransitionAction(true, true, false, true) == OverviewEditTransitionAction::Defer,
                 "gesture workspace transitions should continue deferring focus or movement edits");
    ok &= expect(resolveOverviewEditTransitionAction(true, true, true, false) == OverviewEditTransitionAction::Defer,
                 "other overview modes should continue deferring focus or movement edits");
    ok &= expect(resolveOverviewEditTransitionAction(true, false, true, true) == OverviewEditTransitionAction::Run,
                 "unrelated dispatchers should preserve their existing transition handling");
    ok &= expect(resolveOverviewRebuildTransitionAction(false, true, false) == OverviewRebuildTransitionAction::Rebuild,
                 "overview rebuilds should run when no workspace transition is active");
    ok &= expect(resolveOverviewRebuildTransitionAction(true, false, true) == OverviewRebuildTransitionAction::Rebuild,
                 "multi-workspace overviews should rebuild during workspace transitions");
    ok &= expect(resolveOverviewRebuildTransitionAction(true, true, false) == OverviewRebuildTransitionAction::Discard,
                 "single-workspace rebuilds queued before a transition should be discarded");
    ok &= expect(resolveOverviewRebuildTransitionAction(true, true, true) == OverviewRebuildTransitionAction::DeferUntilCommit,
                 "single-workspace rebuilds queued during a transition should wait for its commit");

    ok &= expect(parseWorkspaceStripAnchor("top") == WorkspaceStripAnchor::Top, "top anchor should parse");
    ok &= expect(parseWorkspaceStripAnchor(" LEFT ") == WorkspaceStripAnchor::Left, "left anchor parsing should ignore case and whitespace");
    ok &= expect(parseWorkspaceStripAnchor("Right") == WorkspaceStripAnchor::Right, "right anchor should parse");
    ok &= expect(parseWorkspaceStripAnchor("unexpected") == WorkspaceStripAnchor::Top, "invalid anchors should fall back to top");
    ok &= expect(parseWorkspaceStripEmptyMode("existing") == WorkspaceStripEmptyMode::Existing, "existing empty-mode should parse");
    ok &= expect(parseWorkspaceStripEmptyMode(" Continuous ") == WorkspaceStripEmptyMode::Continuous,
                 "continuous empty-mode parsing should ignore case and whitespace");
    ok &= expect(parseWorkspaceStripEmptyMode("unexpected") == WorkspaceStripEmptyMode::Existing, "invalid empty-mode should fall back to existing");

    ok &= expect(parseHymissionScrollMode("layout") == std::optional<HymissionScrollMode>{HymissionScrollMode::Layout}, "layout niri scroll mode should parse");
    ok &= expect(!parseHymissionScrollMode(" workspace ").has_value(), "workspace should stay on Hyprland's standard workspace gesture");
    ok &= expect(!parseHymissionScrollMode("Both").has_value(), "combined layout/workspace scroll mode should not parse");
    ok &= expect(!parseHymissionScrollMode("unexpected").has_value(), "invalid niri scroll mode should fail");

    ok &= expect(parseScrollingLayoutDirection("left") == ScrollingLayoutDirection::Left, "left scrolling direction should parse");
    ok &= expect(parseScrollingLayoutDirection("DOWN") == ScrollingLayoutDirection::Down, "down scrolling direction parsing should ignore case");
    ok &= expect(parseScrollingLayoutDirection(" up ") == ScrollingLayoutDirection::Up, "up scrolling direction parsing should ignore whitespace");

    ok &= expect(workspaceAxisForAnimationStyle("slide 100%") == GestureAxis::Horizontal, "slide workspace style should be horizontal");
    ok &= expect(workspaceAxisForAnimationStyle("slidefade 20%") == GestureAxis::Horizontal, "slidefade workspace style should be horizontal");
    ok &= expect(workspaceAxisForAnimationStyle("SlideVert 100%") == GestureAxis::Vertical, "slidevert workspace style should be vertical");
    ok &= expect(workspaceAxisForAnimationStyle("slidefadevert") == GestureAxis::Vertical, "slidefadevert workspace style should be vertical");
    ok &= expect(workspaceAxisForAnimationStyle("slide top") == GestureAxis::Vertical, "slide top workspace style should be vertical");
    ok &= expect(workspaceAxisForAnimationStyle("slidevert left") == GestureAxis::Horizontal, "explicit left suffix should be horizontal");
    ok &= expect(workspaceAxisForAnimationStyle("") == GestureAxis::Horizontal, "empty workspace style should default to horizontal");
    ok &= expect(resolveWorkspaceLaneStackAxis(false, GestureAxis::Horizontal, GestureAxis::Horizontal) == GestureAxis::Horizontal,
                 "non-scrolling horizontal workspace axis should stack lanes horizontally");
    ok &= expect(resolveWorkspaceLaneStackAxis(false, GestureAxis::Horizontal, GestureAxis::Vertical) == GestureAxis::Vertical,
                 "non-scrolling vertical workspace axis should stack lanes vertically");
    ok &= expect(resolveWorkspaceLaneStackAxis(true, GestureAxis::Horizontal, GestureAxis::Vertical) == GestureAxis::Vertical,
                 "horizontal scrolling with vertical workspace axis should stack lanes vertically");
    ok &= expect(resolveWorkspaceLaneStackAxis(true, GestureAxis::Horizontal, GestureAxis::Horizontal) == GestureAxis::Vertical,
                 "horizontal scrolling must never stack lanes horizontally");
    ok &= expect(resolveWorkspaceLaneStackAxis(true, GestureAxis::Vertical, GestureAxis::Vertical) == GestureAxis::Horizontal,
                 "vertical scrolling must never stack lanes vertically");
    ok &= expect(resolveWorkspaceLaneStackAxis(true, GestureAxis::Vertical, GestureAxis::Horizontal) == GestureAxis::Horizontal,
                 "vertical scrolling with horizontal workspace axis should stack lanes horizontally");
    ok &= expect(parseScrollingLayoutDirection("unexpected") == ScrollingLayoutDirection::Right, "invalid scrolling direction should fall back to right");

    ok &= expect(axisForScrollingLayoutDirection(ScrollingLayoutDirection::Right) == GestureAxis::Horizontal, "right scrolling direction should use horizontal gestures");
    ok &= expect(axisForScrollingLayoutDirection(ScrollingLayoutDirection::Left) == GestureAxis::Horizontal, "left scrolling direction should use horizontal gestures");
    ok &= expect(axisForScrollingLayoutDirection(ScrollingLayoutDirection::Down) == GestureAxis::Vertical, "down scrolling direction should use vertical gestures");
    ok &= expect(axisForScrollingLayoutDirection(ScrollingLayoutDirection::Up) == GestureAxis::Vertical, "up scrolling direction should use vertical gestures");
    ok &= expect(scrollingLayoutGestureAxisMatches(ScrollingLayoutDirection::Down, GestureAxis::Vertical),
                 "vertical gestures should match down scrolling layouts");
    ok &= expect(!scrollingLayoutGestureAxisMatches(ScrollingLayoutDirection::Down, GestureAxis::Horizontal),
                 "horizontal gestures should not match down scrolling layouts");
    ok &= expect(closeEnough(scrollingLayoutMoveAmount(ScrollingLayoutDirection::Right, -12.0, 2.0), -24.0),
                 "right scrolling move amount should preserve gesture sign");
    ok &= expect(closeEnough(scrollingLayoutMoveAmount(ScrollingLayoutDirection::Left, -12.0, 2.0), 24.0),
                 "left scrolling move amount should invert gesture sign");
    ok &= expect(closeEnough(scrollingLayoutMoveAmount(ScrollingLayoutDirection::Down, 5.0, 3.0), 15.0),
                 "down scrolling move amount should preserve gesture sign");
    ok &= expect(closeEnough(scrollingLayoutMoveAmount(ScrollingLayoutDirection::Up, 5.0, 3.0), -15.0),
                 "up scrolling move amount should invert gesture sign");
    ok &= expect(closeEnough(scrollingLayoutMoveAmount(ScrollingLayoutDirection::Right, 5.0, -3.0), 0.0),
                 "negative niri scroll sensitivity should clamp to zero");
    ok &= expect(closeEnough(niriOverviewPreviewScale({0, 0, 900, 500}, {0, 0, 3000, 500}, 0.95, 0.10, GestureAxis::Horizontal), 0.95),
                 "horizontal niri overview should ignore scrolling-tape width when fitting scale");
    ok &= expect(closeEnough(niriOverviewPreviewScale({0, 0, 900, 500}, {0, 0, 3000, 500}, 0.95, 0.10), 0.30),
                 "normal niri overview fitting should still account for both axes");
    ok &= expect(closeEnough(niriOverviewPreviewScale({0, 0, 900, 500}, {0, 0, 900, 2400}, 0.95, 0.10, GestureAxis::Vertical), 0.95),
                 "vertical niri overview should ignore scrolling-tape height when fitting scale");
    ok &= expect(closeEnough(niriOverviewPreviewScale({0, 0, 900, 360}, {0, 0, 900, 500}, 0.95, 0.10, GestureAxis::Horizontal), 0.72),
                 "niri overview should still shrink when the cross axis cannot fit the configured max scale");
    ok &= expectRect(centerAnchorOnWorkspaceStripAxis({100, 100, 80, 40}, {300, 240, 120, 60}, WorkspaceStripAnchor::Left), {100, 250, 80, 40},
                     "side strip anchors should center on the focused preview y-axis");
    ok &= expectRect(centerAnchorOnWorkspaceStripAxis({100, 100, 80, 40}, {300, 240, 120, 60}, WorkspaceStripAnchor::Top), {320, 100, 80, 40},
                     "top strip anchors should center on the focused preview x-axis");

    ok &= expect(isWorkspaceStripHorizontal(WorkspaceStripAnchor::Top), "top strip should be horizontal");
    ok &= expect(!isWorkspaceStripHorizontal(WorkspaceStripAnchor::Left), "left strip should be vertical");
    ok &= expect(!isWorkspaceStripHorizontal(WorkspaceStripAnchor::Right), "right strip should be vertical");

    ok &= expect(expandWorkspaceStripWorkspaceIds({1, 5, 9}, WorkspaceStripEmptyMode::Existing) == std::vector<int64_t>({1, 5, 9}),
                 "existing empty-mode should keep sparse workspace ids");
    ok &= expect(expandWorkspaceStripWorkspaceIds({9, 1, 5, 5}, WorkspaceStripEmptyMode::Continuous) == std::vector<int64_t>({1, 2, 5, 6, 9}),
                 "continuous empty-mode should progressively expose numeric gaps");
    ok &= expect(expandWorkspaceStripWorkspaceIds({1, 1000}, WorkspaceStripEmptyMode::Continuous) == std::vector<int64_t>({1, 2, 1000}),
                 "continuous empty-mode should not expand large numeric spans into one slot per id");
    ok &= expect(expandWorkspaceStripWorkspaceIds({-1337, 1}, WorkspaceStripEmptyMode::Continuous) == std::vector<int64_t>({-1337, 1}),
                 "continuous empty-mode should not expand named workspace ids");
    ok &= expect(expandWorkspaceStripWorkspaceIds({}, WorkspaceStripEmptyMode::Continuous).empty(),
                 "empty workspace id sets should stay empty");
    ok &= expect(niriEmptyWorkspaceLaneIds({2, 3}, {2, 3}, WorkspaceStripEmptyMode::Continuous) == std::vector<int64_t>({1, 2, 3, 4}),
                 "niri lanes should keep one empty leaf workspace on each occupied edge");
    ok &= expect(niriEmptyWorkspaceLaneIds({1, 3}, {1, 3}, WorkspaceStripEmptyMode::Continuous) == std::vector<int64_t>({1, 2, 3, 4}),
                 "niri lanes should keep occupied gaps and the available outer leaf");
    ok &= expect(niriEmptyWorkspaceLaneIds({1}, {3}, WorkspaceStripEmptyMode::Continuous) == std::vector<int64_t>({1, 2, 3, 4}),
                 "niri lanes should include a newly occupied unvisited destination");
    ok &= expect(niriEmptyWorkspaceLaneIds({2}, {}, WorkspaceStripEmptyMode::Continuous) == std::vector<int64_t>({2}),
                 "a visited empty workspace should not manufacture adjacent leaves");
    ok &= expect(niriEmptyWorkspacePlaceholderRange({1, 2, 3, 4}, {2, 3}, 2) == std::pair<int64_t, int64_t>{1, 4},
                 "niri placeholders should include occupied gaps and both leaf workspaces");
    ok &= expect(niriEmptyWorkspacePlaceholderRange({1, 2, 3, 4}, {1}, 3) == std::pair<int64_t, int64_t>{1, 4},
                 "niri placeholders should keep a visited empty workspace and the occupied outer leaf");
    ok &= expect(niriEmptyWorkspacePlaceholderRange({1, 2, 3}, {}, 2) == std::pair<int64_t, int64_t>{2, 2},
                 "niri placeholders should show a visited empty workspace without adjacent cards");
    ok &= expect(!niriEmptyWorkspacePlaceholderRange({1, 2, 3}, {}, std::nullopt),
                 "niri placeholders should stay hidden without an occupied or visited workspace");

    ok &= expectReservation(reserveWorkspaceStripBand({10, 20, 300, 200}, WorkspaceStripAnchor::Top, 40, 12),
                            {
                                {10, 20, 300, 40},
                                {10, 72, 300, 148},
                            },
                            "top strip reservation should reserve band from top edge");
    ok &= expectReservation(reserveWorkspaceStripBand({10, 20, 300, 200}, WorkspaceStripAnchor::Left, 40, 12),
                            {
                                {10, 20, 40, 200},
                                {62, 20, 248, 200},
                            },
                            "left strip reservation should reserve band from left edge");
    ok &= expectReservation(reserveWorkspaceStripBand({10, 20, 300, 200}, WorkspaceStripAnchor::Right, 40, 12),
                            {
                                {270, 20, 40, 200},
                                {10, 20, 248, 200},
                            },
                            "right strip reservation should reserve band from right edge");
    ok &= expectReservation(reserveWorkspaceStripBand({10, 20, 300, 200}, WorkspaceStripAnchor::Top, 0, 24),
                            {
                                {10, 20, 300, 0},
                                {10, 20, 300, 200},
                            },
                            "zero-thickness strips should not reserve content space");
    ok &= expectReservation(reserveWorkspaceStripBand({10, 20, 300, 80}, WorkspaceStripAnchor::Top, 120, 40),
                            {
                                {10, 20, 300, 80},
                                {10, 100, 300, 0},
                            },
                            "strip thickness should clamp to monitor size");

    const auto topSlots = layoutWorkspaceStripSlots({0, 0, 300, 36}, WorkspaceStripAnchor::Top, 3, 15);
    ok &= expect(topSlots.size() == 3, "top strip layout should return one rect per slot");
    ok &= expectRect(topSlots[0], {0, 0, 90, 36}, "top strip first slot should start at left edge");
    ok &= expectRect(topSlots[1], {105, 0, 90, 36}, "top strip second slot should advance along x");
    ok &= expectRect(topSlots[2], {210, 0, 90, 36}, "top strip third slot should end at right edge");

    const auto sideSlots = layoutWorkspaceStripSlots({12, 24, 48, 300}, WorkspaceStripAnchor::Left, 3, 15);
    ok &= expect(sideSlots.size() == 3, "side strip layout should return one rect per slot");
    ok &= expectRect(sideSlots[0], {12, 24, 48, 90}, "side strip first slot should start at top edge");
    ok &= expectRect(sideSlots[1], {12, 129, 48, 90}, "side strip second slot should advance along y");
    ok &= expectRect(sideSlots[2], {12, 234, 48, 90}, "side strip third slot should end at bottom edge");
    ok &= expect(layoutWorkspaceStripSlots({0, 0, 120, 20}, WorkspaceStripAnchor::Top, 0, 10).empty(), "zero-slot strip layout should be empty");

    const auto niriTopSlots = layoutNiriWorkspaceStripSlots({0, 0, 500, 80}, WorkspaceStripAnchor::Top, 3, std::optional<std::size_t>{1}, 10, 8, 2.0);
    ok &= expect(niriTopSlots.size() == 3, "niri top strip layout should return one rect per slot");
    ok &= expectRect(niriTopSlots[0], {48, 8, 128, 64}, "niri top strip should keep active slot centered");
    ok &= expectRect(niriTopSlots[1], {186, 8, 128, 64}, "niri top strip active slot should be centered in the band");
    ok &= expectRect(niriTopSlots[2], {324, 8, 128, 64}, "niri top strip should advance by workspace aspect plus gap");

    const auto niriSideSlots = layoutNiriWorkspaceStripSlots({0, 0, 100, 500}, WorkspaceStripAnchor::Left, 3, std::optional<std::size_t>{1}, 10, 8, 2.0);
    ok &= expect(niriSideSlots.size() == 3, "niri side strip layout should return one rect per slot");
    ok &= expectRect(niriSideSlots[0], {8, 177, 84, 42}, "niri side strip should use monitor aspect thumbnails");
    ok &= expectRect(niriSideSlots[1], {8, 229, 84, 42}, "niri side strip active slot should be centered in the band");
    ok &= expectRect(niriSideSlots[2], {8, 281, 84, 42}, "niri side strip should advance vertically by thumbnail height plus gap");

    const auto niriScaledSideSlots =
        layoutNiriWorkspaceStripSlots({0, 0, 1000, 1000}, WorkspaceStripAnchor::Left, 3, std::optional<std::size_t>{1}, 50, 0, 16.0 / 9.0, 0.5);
    ok &= expect(niriScaledSideSlots.size() == 3, "niri scaled strip layout should return one rect per slot");
    ok &= expectRect(niriScaledSideSlots[0], {250, 28.125, 500, 281.25}, "niri scaled strip should keep monitor ratio at configured strip scale");
    ok &= expectRect(niriScaledSideSlots[1], {250, 359.375, 500, 281.25}, "niri scaled strip should center active workspace without fitting all slots");

    const auto niriOverflowSlots =
        layoutNiriWorkspaceStripSlots({0, 0, 500, 80}, WorkspaceStripAnchor::Top, 6, std::optional<std::size_t>{3}, 10, 8, 2.0);
    ok &= expect(niriOverflowSlots.size() == 6, "niri overflowing strip layout should keep all slots");
    ok &= expectRect(niriOverflowSlots[0], {-228, 8, 128, 64}, "niri overflowing strip should allow slots before the screen");
    ok &= expectRect(niriOverflowSlots[3], {186, 8, 128, 64}, "niri overflowing strip should keep active slot centered");
    ok &= expectRect(niriOverflowSlots[5], {462, 8, 128, 64}, "niri overflowing strip should allow slots after the screen");

    ok &= expect(hitTestWorkspaceStrip(topSlots, 120, 10) == std::optional<std::size_t>{1}, "strip hit-test should find the matching slot");
    ok &= expect(!hitTestWorkspaceStrip(topSlots, 100, 10).has_value(), "strip hit-test should miss strip gaps");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
