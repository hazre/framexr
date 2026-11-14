// FrameXR
//
// Copyright (C) 2026 Nyabsi <nyabsi@sovellus.cc>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <optional>

#include <Eigen/Geometry>

#include "Error.hpp"
#include "KwinProtocol.hpp"
#include "WaylandConnection.hpp"
#include "WindowTracker.hpp"

class KwinWindowTracker final : public WindowTracker
{
  public:
    static auto Open(const AppWindow& window) -> Result<std::unique_ptr<KwinWindowTracker>>;

    ~KwinWindowTracker() override;

    [[nodiscard]] auto Node() const -> uint32_t { return m_stream.node.value_or(0); }

    auto Bounds() -> std::optional<Eigen::AlignedBox2f> override;
    auto Activate() -> void override;

    KwinWindowTracker(const KwinWindowTracker&) = delete;
    auto operator=(const KwinWindowTracker&) -> KwinWindowTracker& = delete;

  private:
    explicit KwinWindowTracker(std::unique_ptr<WaylandConnection> connection);

    std::unique_ptr<WaylandConnection> m_connection;
    org_kde_plasma_window_management* m_management = nullptr;
    zkde_screencast_unstable_v1* m_screencast = nullptr;
    KwinWindow m_window;
    KwinStream m_stream;
};
