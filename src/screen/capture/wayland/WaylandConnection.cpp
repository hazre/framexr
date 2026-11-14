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

#include "WaylandConnection.hpp"

#include <algorithm>
#include <format>

#include <poll.h>
#include <wayland-client.h>

auto WaylandConnection::Connect() -> Result<std::unique_ptr<WaylandConnection>>
{
    static constexpr wl_registry_listener kRegistryListener{
        .global = &WaylandConnection::OnGlobal,
        .global_remove = &WaylandConnection::OnGlobalRemove,
    };

    auto connection = std::unique_ptr<WaylandConnection>(new WaylandConnection());
    connection->m_display = wl_display_connect(nullptr);
    if (connection->m_display == nullptr)
    {
        return Unexpected("Failed to connect to the Wayland display");
    }
    connection->m_registry = wl_display_get_registry(connection->m_display);
    wl_registry_add_listener(connection->m_registry, &kRegistryListener, connection.get());
    RETURN_IF_ERROR(connection->Roundtrip());
    return connection;
}

WaylandConnection::~WaylandConnection()
{
    if (m_registry != nullptr)
    {
        wl_registry_destroy(m_registry);
    }
    if (m_display != nullptr)
    {
        wl_display_disconnect(m_display);
    }
}

auto WaylandConnection::Has(std::string_view interface) const -> bool
{
    return std::ranges::contains(m_globals, interface, &WaylandGlobal::interface);
}

auto WaylandConnection::BindGlobal(const wl_interface& interface, uint32_t max_version) -> void*
{
    const auto global = std::ranges::find(m_globals, std::string_view(interface.name), &WaylandGlobal::interface);
    if (global == m_globals.end())
    {
        return nullptr;
    }
    return wl_registry_bind(m_registry, global->name, &interface, std::min(global->version, max_version));
}

auto WaylandConnection::Roundtrip() -> Result<>
{
    if (wl_display_roundtrip(m_display) < 0)
    {
        return Unexpected(std::format("Wayland roundtrip failed: {}", wl_display_get_error(m_display)));
    }
    return {};
}

auto WaylandConnection::Dispatch(std::chrono::milliseconds timeout) -> Result<bool>
{
    while (wl_display_prepare_read(m_display) != 0)
    {
        wl_display_dispatch_pending(m_display);
    }
    wl_display_flush(m_display);
    pollfd descriptor{.fd = wl_display_get_fd(m_display), .events = POLLIN, .revents = 0};
    if (poll(&descriptor, 1, static_cast<int>(timeout.count())) <= 0)
    {
        wl_display_cancel_read(m_display);
        return false;
    }
    if (wl_display_read_events(m_display) < 0 || wl_display_dispatch_pending(m_display) < 0)
    {
        return Unexpected(std::format("Wayland dispatch failed: {}", wl_display_get_error(m_display)));
    }
    return true;
}

auto WaylandConnection::OnGlobal(void* data, wl_registry*, uint32_t name, const char* interface, uint32_t version) -> void
{
    static_cast<WaylandConnection*>(data)->m_globals.push_back({.name = name, .interface = interface, .version = version});
}

auto WaylandConnection::OnGlobalRemove(void* data, wl_registry*, uint32_t name) -> void
{
    std::erase_if(
        static_cast<WaylandConnection*>(data)->m_globals,
        [name](const auto& global) -> bool { return global.name == name; }
    );
}
