#include <cstdlib>
#include <iostream>

#include "overview_logic.hpp"
#include "snapshot_refresh.hpp"

namespace {

bool expect(bool condition, const char* message) {
    if (condition)
        return true;

    std::cerr << "FAIL: " << message << '\n';
    return false;
}

bool testOverviewSelection() {
    using hymission::chooseOverviewSelectionIndex;
    bool ok = true;
    ok &= expect(!chooseOverviewSelectionIndex(0, std::nullopt, std::nullopt),
                 "an empty overview must not select global focus on another monitor");
    ok &= expect(!chooseOverviewSelectionIndex(0, 0, 0), "stale indices must not select a window in an empty overview");
    ok &= expect(!chooseOverviewSelectionIndex(2, std::nullopt, std::nullopt), "focus outside the overview must not become the selection");
    ok &= expect(chooseOverviewSelectionIndex(2, 0, 1) == 0, "overview selection must outrank desktop focus");
    ok &= expect(chooseOverviewSelectionIndex(2, std::nullopt, 1) == 1, "focus inside the overview remains a valid fallback");
    ok &= expect(chooseOverviewSelectionIndex(2, 5, 1) == 1, "a stale selection may fall back to managed focus");
    ok &= expect(!chooseOverviewSelectionIndex(2, 5, 2), "out-of-range selection and focus must both be rejected");
    return ok;
}

bool testDeferredLayerSnapshots() {
    hymission::LayerSnapshotRefresh refresh;
    using hymission::LayerSnapshotKind;
    bool ok = true;
    refresh.request(LayerSnapshotKind::HiddenStrip);
    refresh.request(LayerSnapshotKind::Wallpaper);
    refresh.request(LayerSnapshotKind::HiddenStrip);
    ok &= expect(refresh.take(true).none() && refresh.pending(), "active rendering must retain all capture requests without running them");
    refresh.request(LayerSnapshotKind::WallpaperLayout);
    ok &= expect(refresh.take(true).none() && refresh.pending(), "a busy deferred retry must preserve newly queued work");
    const auto requests = refresh.take(false);
    ok &= expect(requests.count() == 3 && !refresh.pending(), "idle refresh must coalesce captures and drain all snapshot kinds");
    ok &= expect(refresh.take(false).none(), "a completed refresh must not replay old requests");
    refresh.request(LayerSnapshotKind::Wallpaper);
    refresh.clear();
    ok &= expect(refresh.take(false).none(), "closing an overview must cancel pending captures");
    refresh.request(LayerSnapshotKind::HiddenStrip);
    ok &= expect(refresh.take(false).count() == 1, "a new overview must receive only its own capture requests");
    return ok;
}

} // namespace

int main() {
    const bool selection = testOverviewSelection();
    const bool snapshots = testDeferredLayerSnapshots();
    return selection && snapshots ? EXIT_SUCCESS : EXIT_FAILURE;
}
