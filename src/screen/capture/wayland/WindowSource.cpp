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

#include "WindowSource.hpp"

#include "DesktopEntry.hpp"
#include "KwinWindowSource.hpp"
#include "Logger.hpp"
#include "WaylandConnection.hpp"

auto WindowSource::Create() -> std::shared_ptr<WindowSource>
{
    DesktopEntry::Ensure();
    const auto connection = WaylandConnection::Connect();
    if (!connection)
    {
        LOG_WARN("Window capture unavailable: {}", connection.error().Message());
        return nullptr;
    }
    if (KwinWindowSource::Supported(**connection))
    {
        return std::make_shared<KwinWindowSource>();
    }
    return nullptr;
}
