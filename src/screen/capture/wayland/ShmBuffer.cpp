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

#include "ShmBuffer.hpp"

#include <atomic>
#include <format>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client.h>

#include "Frame.hpp"

auto ShmBuffer::Create(wl_shm* shm, uint32_t width, uint32_t height, uint32_t stride, uint32_t format) -> std::unique_ptr<ShmBuffer>
{
    static std::atomic<uint32_t> counter{0};
    const auto name = std::format("framexr-{}", counter.fetch_add(1));
    const auto size = stride * height;
    auto buffer = std::unique_ptr<ShmBuffer>(new ShmBuffer());
    buffer->m_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    shm_unlink(name.c_str());
    if (buffer->m_fd < 0 || ftruncate(buffer->m_fd, size) != 0)
    {
        return nullptr;
    }
    buffer->m_pool = wl_shm_create_pool(shm, buffer->m_fd, static_cast<int32_t>(size));
    buffer->m_buffer = wl_shm_pool_create_buffer(
        buffer->m_pool,
        0,
        static_cast<int32_t>(width),
        static_cast<int32_t>(height),
        static_cast<int32_t>(stride),
        format
    );
    return buffer;
}

auto ShmBuffer::Fourcc(uint32_t shm_format) -> std::optional<uint32_t>
{
    switch (shm_format)
    {
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_ABGR8888: return kDrmFormatArgb8888;
    case WL_SHM_FORMAT_XRGB8888:
    case WL_SHM_FORMAT_XBGR8888: return kDrmFormatXrgb8888;
    default: return std::nullopt;
    }
}

ShmBuffer::~ShmBuffer()
{
    if (m_buffer != nullptr)
    {
        wl_buffer_destroy(m_buffer);
    }
    if (m_pool != nullptr)
    {
        wl_shm_pool_destroy(m_pool);
    }
    if (m_fd >= 0)
    {
        close(m_fd);
    }
}
