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

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "Error.hpp"
#include "WaylandClient.hpp"

struct wl_interface;

class WaylandConnection final
{
  public:
    static auto Connect() -> Result<std::unique_ptr<WaylandConnection>>;

    ~WaylandConnection();

    [[nodiscard]] auto Display() const -> wl_display* { return m_display; }
    [[nodiscard]] auto Has(std::string_view interface) const -> bool;

    template <typename T>
    auto Bind(const wl_interface& interface, uint32_t max_version) -> T*
    {
        return static_cast<T*>(BindGlobal(interface, max_version));
    }

    auto Roundtrip() -> Result<>;
    auto Dispatch(std::chrono::milliseconds timeout) -> Result<bool>;

    WaylandConnection(const WaylandConnection&) = delete;
    auto operator=(const WaylandConnection&) -> WaylandConnection& = delete;

  private:
    WaylandConnection() = default;

    auto BindGlobal(const wl_interface& interface, uint32_t max_version) -> void*;

    static auto OnGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) -> void;
    static auto OnGlobalRemove(void* data, wl_registry* registry, uint32_t name) -> void;

    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    std::vector<WaylandGlobal> m_globals;
};
