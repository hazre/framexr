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

#include "ExtProtocol.hpp"
#include "WaylandConnection.hpp"
#include "WindowSource.hpp"

class ExtWindowSource final : public WindowSource
{
  public:
    static auto Supported(const WaylandConnection& connection) -> bool;
    static auto List(WaylandConnection& connection, ExtToplevelList& list) -> Result<>;
    static auto Release(ExtToplevelList& list) -> void;

    auto Windows() -> Result<std::vector<AppWindow>> override;
    auto Open(const AppWindow& window) -> std::future<Result<std::unique_ptr<Capture>>> override;
};
