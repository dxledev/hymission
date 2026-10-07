#pragma once

#include <bitset>
#include <cstddef>
#include <utility>

namespace hymission {

enum class LayerSnapshotKind {
    HiddenStrip,
    Wallpaper,
    WallpaperLayout,
    Count,
};

class LayerSnapshotRefresh {
  public:
    using Requests = std::bitset<static_cast<std::size_t>(LayerSnapshotKind::Count)>;

    void request(LayerSnapshotKind kind) {
        m_pending.set(static_cast<std::size_t>(kind));
    }

    [[nodiscard]] bool pending() const {
        return m_pending.any();
    }

    [[nodiscard]] Requests take(bool renderContextActive) {
        if (renderContextActive)
            return {};

        return std::exchange(m_pending, {});
    }

    void clear() {
        m_pending.reset();
    }

  private:
    Requests m_pending;
};

} // namespace hymission
