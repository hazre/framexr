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

#include "WgcWindowSource.hpp"

#include <cstdint>
#include <format>
#include <utility>

#include <dwmapi.h>

#include "Utf16.hpp"
#include "WgcCapture.hpp"

auto WgcWindowSource::Windows() -> Result<std::vector<AppWindow>>
{
    std::vector<AppWindow> windows;
    m_handles.clear();
    for (HWND window = GetTopWindow(nullptr); window != nullptr; window = GetWindow(window, GW_HWNDNEXT))
    {
        if (!Listed(window))
        {
            continue;
        }
        const int length = GetWindowTextLengthW(window);
        std::wstring title(static_cast<size_t>(length) + 1, L'\0');
        title.resize(static_cast<size_t>(GetWindowTextW(window, title.data(), length + 1)));
        auto& listed = windows.emplace_back(
            AppWindow{.id = std::to_string(reinterpret_cast<uintptr_t>(window)), .title = Utf16::Narrow(title), .app_id = {}}
        );
        m_handles.emplace(listed.id, window);
    }
    return windows;
}

auto WgcWindowSource::Open(const AppWindow& window) -> std::future<Result<std::unique_ptr<Capture>>>
{
    const auto handle = m_handles.find(window.id);
    return std::async(
        std::launch::async,
        [title = window.title, hwnd = handle != m_handles.end() ? handle->second : nullptr]() -> Result<std::unique_ptr<Capture>>
        {
            if (hwnd == nullptr || IsWindow(hwnd) == FALSE)
            {
                return Unexpected(std::format("{} is no longer open", title));
            }
            return std::make_unique<WgcCapture>(title, hwnd);
        }
    );
}

auto WgcWindowSource::Listed(HWND window) -> bool
{
    BOOL cloaked = FALSE;
    return IsWindowVisible(window) != FALSE && GetWindow(window, GW_OWNER) == nullptr && GetWindowTextLengthW(window) > 0
        && (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) == 0
        && (FAILED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) || cloaked == FALSE);
}
