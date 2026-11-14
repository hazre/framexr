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

#include "HwndTracker.hpp"

#include <dwmapi.h>

auto HwndTracker::Bounds() -> std::optional<Eigen::AlignedBox2f>
{
    RECT rect{};
    if (IsWindow(m_window) == FALSE || IsIconic(m_window) != FALSE
        || (FAILED(DwmGetWindowAttribute(m_window, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect)))
            && GetWindowRect(m_window, &rect) == FALSE))
    {
        return std::nullopt;
    }
    return Eigen::AlignedBox2f(
        Eigen::Vector2f(static_cast<float>(rect.left), static_cast<float>(rect.top)),
        Eigen::Vector2f(static_cast<float>(rect.right), static_cast<float>(rect.bottom))
    );
}

auto HwndTracker::Activate() -> void
{
    if (GetForegroundWindow() == m_window)
    {
        return;
    }
    if (IsIconic(m_window) != FALSE)
    {
        ShowWindow(m_window, SW_RESTORE);
    }
    SetForegroundWindow(m_window);
}
