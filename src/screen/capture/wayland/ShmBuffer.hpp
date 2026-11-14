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
#include <memory>
#include <optional>

struct wl_buffer;
struct wl_shm;
struct wl_shm_pool;

class ShmBuffer final
{
  public:
    static auto Create(wl_shm* shm, uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
        -> std::unique_ptr<ShmBuffer>;
    static auto Fourcc(uint32_t shm_format) -> std::optional<uint32_t>;

    ~ShmBuffer();

    [[nodiscard]] auto Buffer() const -> wl_buffer* { return m_buffer; }
    [[nodiscard]] auto Fd() const -> int { return m_fd; }

    ShmBuffer(const ShmBuffer&) = delete;
    auto operator=(const ShmBuffer&) -> ShmBuffer& = delete;

  private:
    ShmBuffer() = default;

    wl_shm_pool* m_pool = nullptr;
    wl_buffer* m_buffer = nullptr;
    int m_fd = -1;
};
