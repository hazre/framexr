// FrameXR
//
// This is a port of wlx-overlay-s from Rust to C++.
//
// Copyright (C) 2026 Nyabsi <nyabsi@sovellus.cc>
// Copyright (C) 2023-2025 galister and wlx-overlay-s contributors
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

#include <future>
#include <memory>
#include <optional>
#include <string>

#include <Eigen/Geometry>
#include <vulkan/vulkan.h>

#include "Capture.hpp"
#include "Error.hpp"
#include "ScreenInteraction.hpp"
#include "View.hpp"

class Mirror final : public View
{
  public:
    Mirror(std::string name, std::future<Result<std::unique_ptr<Capture>>> capture);

    auto Switch(std::future<Result<std::unique_ptr<Capture>>> capture) -> void;
    auto Update() -> void override;
    auto OnHover(const PointerHit& hit) -> std::optional<Haptics> override;
    auto OnPointer(const PointerHit& hit, bool pressed) -> void override;
    auto OnScroll(const PointerHit& hit, float delta_y, float delta_x) -> void override;

  private:
    auto Attach(Result<std::unique_ptr<Capture>> capture) -> void;
    auto Input() -> ScreenInteraction*;

    std::optional<std::future<Result<std::unique_ptr<Capture>>>> m_capture;
    VkExtent2D m_extent{};
    WindowTracker* m_window = nullptr;
    std::optional<ScreenInteraction> m_input;
    Eigen::AlignedBox2f m_bounds;
    bool m_attached = false;
};
