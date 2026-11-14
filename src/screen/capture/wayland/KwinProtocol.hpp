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

#include <cstdint>
#include <optional>
#include <string>

#include <Eigen/Geometry>

#include "WindowSource.hpp"
#include "plasma-window-management-client-protocol.h"
#include "zkde-screencast-unstable-v1-client-protocol.h"

struct KwinWindow
{
    org_kde_plasma_window* proxy = nullptr;
    AppWindow window;
    uint32_t state = 0;
    std::optional<Eigen::AlignedBox2f> bounds;
};

struct KwinStream
{
    zkde_screencast_stream_unstable_v1* proxy = nullptr;
    std::optional<uint32_t> node;
    std::optional<std::string> error;
};

constexpr uint32_t kKwinManagementVersion = 16;
constexpr uint32_t kKwinScreencastVersion = 5;

inline constexpr org_kde_plasma_window_listener kKwinWindowListener{
    .title_changed = [](void* data, org_kde_plasma_window*, const char* title) -> void
    { static_cast<KwinWindow*>(data)->window.title = title; },
    .app_id_changed = [](void* data, org_kde_plasma_window*, const char* app_id) -> void
    { static_cast<KwinWindow*>(data)->window.app_id = app_id; },
    .state_changed = [](void* data, org_kde_plasma_window*, uint32_t flags) -> void
    { static_cast<KwinWindow*>(data)->state = flags; },
    .virtual_desktop_changed = [](void*, org_kde_plasma_window*, int32_t) -> void {},
    .themed_icon_name_changed = [](void*, org_kde_plasma_window*, const char*) -> void {},
    .unmapped = [](void*, org_kde_plasma_window*) -> void {},
    .initial_state = [](void*, org_kde_plasma_window*) -> void {},
    .parent_window = [](void*, org_kde_plasma_window*, org_kde_plasma_window*) -> void {},
    .geometry = [](void* data, org_kde_plasma_window*, int32_t x, int32_t y, uint32_t width, uint32_t height) -> void
    {
        const Eigen::Vector2f origin(static_cast<float>(x), static_cast<float>(y));
        static_cast<KwinWindow*>(data)->bounds.emplace(
            origin,
            origin + Eigen::Vector2f(static_cast<float>(width), static_cast<float>(height))
        );
    },
    .icon_changed = [](void*, org_kde_plasma_window*) -> void {},
    .pid_changed = [](void*, org_kde_plasma_window*, uint32_t) -> void {},
    .virtual_desktop_entered = [](void*, org_kde_plasma_window*, const char*) -> void {},
    .virtual_desktop_left = [](void*, org_kde_plasma_window*, const char*) -> void {},
    .application_menu = [](void*, org_kde_plasma_window*, const char*, const char*) -> void {},
    .activity_entered = [](void*, org_kde_plasma_window*, const char*) -> void {},
    .activity_left = [](void*, org_kde_plasma_window*, const char*) -> void {},
    .resource_name_changed = [](void*, org_kde_plasma_window*, const char*) -> void {},
    .client_geometry = [](void*, org_kde_plasma_window*, int32_t, int32_t, uint32_t, uint32_t) -> void {},
    .mapped = [](void*, org_kde_plasma_window*) -> void {},
};

inline constexpr zkde_screencast_stream_unstable_v1_listener kKwinStreamListener{
    .closed = [](void* data, zkde_screencast_stream_unstable_v1*) -> void
    { static_cast<KwinStream*>(data)->error = "The window was closed"; },
    .created = [](void* data, zkde_screencast_stream_unstable_v1*, uint32_t node) -> void
    { static_cast<KwinStream*>(data)->node = node; },
    .failed = [](void* data, zkde_screencast_stream_unstable_v1*, const char* error) -> void
    { static_cast<KwinStream*>(data)->error = error; },
    .serial = [](void*, zkde_screencast_stream_unstable_v1*, uint32_t, uint32_t) -> void {},
};
