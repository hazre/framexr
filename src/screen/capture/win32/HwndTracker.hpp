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

#include <optional>

#include <Eigen/Geometry>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "WindowTracker.hpp"

class HwndTracker final : public WindowTracker
{
  public:
    explicit HwndTracker(HWND window) : m_window(window) {}

    auto Bounds() -> std::optional<Eigen::AlignedBox2f> override;
    auto Activate() -> void override;

  private:
    HWND m_window;
};
