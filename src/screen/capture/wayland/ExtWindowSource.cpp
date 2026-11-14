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

#include "ExtWindowSource.hpp"

#include <wayland-client.h>

#include "ExtWindowCapture.hpp"
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"

auto ExtWindowSource::Supported(const WaylandConnection& connection) -> bool
{
    return connection.Has(ext_foreign_toplevel_list_v1_interface.name)
        && connection.Has(ext_foreign_toplevel_image_capture_source_manager_v1_interface.name)
        && connection.Has(ext_image_copy_capture_manager_v1_interface.name);
}

auto ExtWindowSource::List(WaylandConnection& connection, ExtToplevelList& list) -> Result<>
{
    list.proxy = connection.Bind<ext_foreign_toplevel_list_v1>(ext_foreign_toplevel_list_v1_interface, 1);
    if (list.proxy == nullptr)
    {
        return Unexpected("The compositor does not share its window list");
    }
    ext_foreign_toplevel_list_v1_add_listener(list.proxy, &kExtListListener, &list);
    RETURN_IF_ERROR(connection.Roundtrip());
    return connection.Roundtrip();
}

auto ExtWindowSource::Release(ExtToplevelList& list) -> void
{
    for (const auto& toplevel : list.toplevels)
    {
        ext_foreign_toplevel_handle_v1_destroy(toplevel.proxy);
    }
    list.toplevels.clear();
    if (list.proxy != nullptr)
    {
        ext_foreign_toplevel_list_v1_stop(list.proxy);
        ext_foreign_toplevel_list_v1_destroy(list.proxy);
        list.proxy = nullptr;
    }
}

auto ExtWindowSource::Windows() -> Result<std::vector<AppWindow>>
{
    ASSIGN_OR_RETURN(auto connection, WaylandConnection::Connect());
    ExtToplevelList list;
    const auto listed = List(*connection, list);
    std::vector<AppWindow> windows;
    for (const auto& toplevel : list.toplevels)
    {
        if (!toplevel.closed && !toplevel.window.title.empty())
        {
            windows.push_back(toplevel.window);
        }
    }
    Release(list);
    wl_display_flush(connection->Display());
    RETURN_IF_ERROR(listed);
    return windows;
}

auto ExtWindowSource::Open(const AppWindow& window) -> std::future<Result<std::unique_ptr<Capture>>>
{
    return std::async(std::launch::async, [window]() -> Result<std::unique_ptr<Capture>> { return ExtWindowCapture::Open(window); });
}
