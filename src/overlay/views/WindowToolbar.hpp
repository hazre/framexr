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
#include <string_view>

#include <Eigen/Core>

#include "ActionRunner.hpp"
#include "Error.hpp"
#include "View.hpp"

constexpr std::string_view kWindowToolbarName = "window-toolbar";

class WindowToolbar final : public View
{
  public:
    static auto Create() -> Result<std::unique_ptr<WindowToolbar>>;

    auto Update() -> void override;

  private:
    WindowToolbar() = default;

    auto Build(bool app) -> Result<>;
    auto FindTarget() -> View*;
    auto Place(View& target) -> bool;
    auto Act(const OverlayAction& action) -> void;

    ActionRunner m_runner;
    std::optional<OverlayId> m_target;
    std::optional<bool> m_app;
    Eigen::Vector2f m_size = Eigen::Vector2f::Zero();
};
