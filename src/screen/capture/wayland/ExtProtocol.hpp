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

#include <deque>

#include "WindowSource.hpp"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

struct ExtToplevel
{
    ext_foreign_toplevel_handle_v1* proxy = nullptr;
    AppWindow window;
    bool closed = false;
};

struct ExtToplevelList
{
    ext_foreign_toplevel_list_v1* proxy = nullptr;
    std::deque<ExtToplevel> toplevels;
};

inline constexpr ext_foreign_toplevel_handle_v1_listener kExtToplevelListener{
    .closed = [](void* data, ext_foreign_toplevel_handle_v1*) -> void { static_cast<ExtToplevel*>(data)->closed = true; },
    .done = [](void*, ext_foreign_toplevel_handle_v1*) -> void {},
    .title = [](void* data, ext_foreign_toplevel_handle_v1*, const char* title) -> void
    { static_cast<ExtToplevel*>(data)->window.title = title; },
    .app_id = [](void* data, ext_foreign_toplevel_handle_v1*, const char* app_id) -> void
    { static_cast<ExtToplevel*>(data)->window.app_id = app_id; },
    .identifier = [](void* data, ext_foreign_toplevel_handle_v1*, const char* identifier) -> void
    { static_cast<ExtToplevel*>(data)->window.id = identifier; },
};

inline constexpr ext_foreign_toplevel_list_v1_listener kExtListListener{
    .toplevel = [](void* data, ext_foreign_toplevel_list_v1*, ext_foreign_toplevel_handle_v1* handle) -> void
    {
        auto& toplevel = static_cast<ExtToplevelList*>(data)->toplevels.emplace_back(ExtToplevel{.proxy = handle});
        ext_foreign_toplevel_handle_v1_add_listener(handle, &kExtToplevelListener, &toplevel);
    },
    .finished = [](void*, ext_foreign_toplevel_list_v1*) -> void {},
};
