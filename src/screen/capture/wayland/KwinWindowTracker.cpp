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

#include "KwinWindowTracker.hpp"

#include <chrono>
#include <format>
#include <utility>

#include <wayland-client.h>

namespace
{

constexpr auto kStreamTimeout = std::chrono::seconds(5);

} // namespace

KwinWindowTracker::KwinWindowTracker(std::unique_ptr<WaylandConnection> connection) : m_connection(std::move(connection))
{
}

auto KwinWindowTracker::Open(const AppWindow& window) -> Result<std::unique_ptr<KwinWindowTracker>>
{
    ASSIGN_OR_RETURN(auto connection, WaylandConnection::Connect());
    auto tracker = std::unique_ptr<KwinWindowTracker>(new KwinWindowTracker(std::move(connection)));
    tracker->m_management = tracker->m_connection->Bind<org_kde_plasma_window_management>(
        org_kde_plasma_window_management_interface,
        kKwinManagementVersion
    );
    tracker->m_screencast = tracker->m_connection->Bind<zkde_screencast_unstable_v1>(
        zkde_screencast_unstable_v1_interface,
        kKwinScreencastVersion
    );
    if (tracker->m_management == nullptr || tracker->m_screencast == nullptr)
    {
        return Unexpected("KWin does not allow FrameXR to capture windows");
    }

    tracker->m_window.proxy = org_kde_plasma_window_management_get_window_by_uuid(tracker->m_management, window.id.c_str());
    org_kde_plasma_window_add_listener(tracker->m_window.proxy, &kKwinWindowListener, &tracker->m_window);
    tracker->m_stream.proxy = zkde_screencast_unstable_v1_stream_window(
        tracker->m_screencast,
        window.id.c_str(),
        ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_EMBEDDED
    );
    zkde_screencast_stream_unstable_v1_add_listener(tracker->m_stream.proxy, &kKwinStreamListener, &tracker->m_stream);

    while (!tracker->m_stream.node && !tracker->m_stream.error)
    {
        ASSIGN_OR_RETURN(const bool read, tracker->m_connection->Dispatch(kStreamTimeout));
        if (!read)
        {
            return Unexpected("Timed out waiting for KWin");
        }
    }
    if (tracker->m_stream.error)
    {
        return Unexpected(std::format("KWin refused to capture {}: {}", window.title, *tracker->m_stream.error));
    }
    return tracker;
}

KwinWindowTracker::~KwinWindowTracker()
{
    if (m_stream.proxy != nullptr)
    {
        zkde_screencast_stream_unstable_v1_close(m_stream.proxy);
    }
    if (m_window.proxy != nullptr)
    {
        org_kde_plasma_window_destroy(m_window.proxy);
    }
    if (m_screencast != nullptr)
    {
        zkde_screencast_unstable_v1_destroy(m_screencast);
    }
    if (m_management != nullptr)
    {
        org_kde_plasma_window_management_destroy(m_management);
    }
    wl_display_flush(m_connection->Display());
}

auto KwinWindowTracker::Bounds() -> std::optional<Eigen::AlignedBox2f>
{
    std::ignore = m_connection->Dispatch(std::chrono::milliseconds(0));
    return m_stream.error ? std::nullopt : m_window.bounds;
}

auto KwinWindowTracker::Activate() -> void
{
    if ((m_window.state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE) != 0)
    {
        return;
    }
    org_kde_plasma_window_set_state(
        m_window.proxy,
        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE | ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED,
        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE
    );
    wl_display_flush(m_connection->Display());
}
