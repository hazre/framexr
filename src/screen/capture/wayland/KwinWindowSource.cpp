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

#include "KwinWindowSource.hpp"

#include <deque>
#include <utility>

#include <wayland-client.h>

#include "KwinProtocol.hpp"
#include "KwinWindowTracker.hpp"
#include "PipewireCapture.hpp"

namespace
{

constexpr uint32_t kHiddenStates = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR
                                 | ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_EXCLUDE_FROM_CAPTURE;

constexpr org_kde_plasma_window_management_listener kManagementListener{
    .show_desktop_changed = [](void*, org_kde_plasma_window_management*, uint32_t) -> void {},
    .window = [](void*, org_kde_plasma_window_management*, uint32_t) -> void {},
    .stacking_order_changed = [](void*, org_kde_plasma_window_management*, wl_array*) -> void {},
    .stacking_order_uuid_changed = [](void*, org_kde_plasma_window_management*, const char*) -> void {},
    .window_with_uuid = [](void* data, org_kde_plasma_window_management*, uint32_t, const char* uuid) -> void
    { static_cast<std::vector<std::string>*>(data)->emplace_back(uuid); },
    .stacking_order_changed_2 = [](void*, org_kde_plasma_window_management*) -> void {},
};

} // namespace

auto KwinWindowSource::Supported(const WaylandConnection& connection) -> bool
{
    return connection.Has(org_kde_plasma_window_management_interface.name)
        && connection.Has(zkde_screencast_unstable_v1_interface.name);
}

auto KwinWindowSource::Windows() -> Result<std::vector<AppWindow>>
{
    ASSIGN_OR_RETURN(auto connection, WaylandConnection::Connect());
    auto* manager = connection->Bind<org_kde_plasma_window_management>(
        org_kde_plasma_window_management_interface,
        kKwinManagementVersion
    );
    if (manager == nullptr)
    {
        return Unexpected("KWin does not share its window list with FrameXR");
    }

    std::vector<std::string> uuids;
    org_kde_plasma_window_management_add_listener(manager, &kManagementListener, &uuids);
    RETURN_IF_ERROR(connection->Roundtrip());

    std::deque<KwinWindow> windows;
    for (const auto& uuid : uuids)
    {
        auto& window = windows.emplace_back(
            KwinWindow{.proxy = org_kde_plasma_window_management_get_window_by_uuid(manager, uuid.c_str()), .window = {.id = uuid}}
        );
        org_kde_plasma_window_add_listener(window.proxy, &kKwinWindowListener, &window);
    }
    const auto listed = connection->Roundtrip();

    std::vector<AppWindow> result;
    for (auto& window : windows)
    {
        if ((window.state & kHiddenStates) == 0 && !window.window.title.empty())
        {
            result.push_back(std::move(window.window));
        }
        org_kde_plasma_window_destroy(window.proxy);
    }
    org_kde_plasma_window_management_destroy(manager);
    RETURN_IF_ERROR(listed);
    return result;
}

auto KwinWindowSource::Open(const AppWindow& window) -> std::future<Result<std::unique_ptr<Capture>>>
{
    return std::async(std::launch::async, [window]() -> Result<std::unique_ptr<Capture>> { return Stream(window); });
}

auto KwinWindowSource::Stream(const AppWindow& window) -> Result<std::unique_ptr<Capture>>
{
    ASSIGN_OR_RETURN(auto tracker, KwinWindowTracker::Open(window));
    const auto node = tracker->Node();
    return std::make_unique<PipewireCapture>(window.title, node, std::move(tracker));
}
