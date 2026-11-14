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

#include "DesktopEntry.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <mutex>
#include <ranges>
#include <string>
#include <system_error>

#include "ConfigStore.hpp"
#include "Logger.hpp"
#include "Platform.hpp"

auto DesktopEntry::Ensure() -> void
{
    static std::once_flag once;
    std::call_once(
        once,
        []() -> void
        {
            const auto directories = DataDirectories();
            const auto file = std::format("applications/{}.desktop", kAppId);
            if (std::ranges::any_of(
                    directories | std::views::drop(1),
                    [&](const auto& directory) -> bool
                    {
                        std::error_code error;
                        return std::filesystem::exists(directory / file, error);
                    }
                ))
            {
                return;
            }
            const auto executable = Platform::ExecutablePath();
            if (!executable)
            {
                return;
            }
            const auto entry = std::format(
                "[Desktop Entry]\nType=Application\nName=FrameXR\nExec={}\nIcon=framexr\nTerminal=true\nNoDisplay=true\n"
                "X-KDE-Wayland-Interfaces=org_kde_plasma_window_management,zkde_screencast_unstable_v1\n",
                executable->string()
            );
            const auto path = directories.front() / file;
            if (ConfigStore::ReadFile(path) == entry)
            {
                return;
            }
            if (const auto written = ConfigStore::WriteFile(path, entry); !written)
            {
                LOG_WARN("Screen permissions will not be remembered: {}", written.error().Message());
            }
        }
    );
}

auto DesktopEntry::DataDirectories() -> std::vector<std::filesystem::path>
{
    const char* home = std::getenv("XDG_DATA_HOME");
    const char* user = std::getenv("HOME");
    std::vector<std::filesystem::path> directories{
        home != nullptr && *home != '\0' ? std::filesystem::path(home)
                                         : std::filesystem::path(user != nullptr ? user : "") / ".local/share"
    };
    const char* dirs = std::getenv("XDG_DATA_DIRS");
    std::string_view list = dirs != nullptr && *dirs != '\0' ? dirs : "/usr/local/share:/usr/share";
    for (const auto directory : std::views::split(list, ':'))
    {
        directories.emplace_back(std::string_view(directory));
    }
    return directories;
}
