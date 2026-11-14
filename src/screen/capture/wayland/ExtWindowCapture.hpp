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

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>

#include "Capture.hpp"
#include "ExtProtocol.hpp"
#include "FrameChannel.hpp"
#include "WaylandConnection.hpp"

struct wl_shm;
struct ext_foreign_toplevel_image_capture_source_manager_v1;
struct ext_image_copy_capture_manager_v1;
struct ext_image_capture_source_v1;
struct ext_image_copy_capture_session_v1;

struct ExtCaptureState
{
    std::unique_ptr<WaylandConnection> connection;
    ExtToplevelList list;
    wl_shm* shm = nullptr;
    ext_foreign_toplevel_image_capture_source_manager_v1* sources = nullptr;
    ext_image_copy_capture_manager_v1* manager = nullptr;
    ext_image_capture_source_v1* source = nullptr;
    ext_image_copy_capture_session_v1* session = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    std::optional<uint32_t> shm_format;
    bool constrained = false;
    bool stopped = false;
};

struct ExtFrameRequest
{
    FrameTransform transform = FrameTransform::Normal;
    std::optional<uint32_t> failure;
    bool ready = false;
    bool finished = false;
};

class ExtWindowCapture final : public Capture
{
  public:
    static auto Open(const AppWindow& window) -> Result<std::unique_ptr<Capture>>;

    ExtWindowCapture(std::string name, std::unique_ptr<ExtCaptureState> state);
    ~ExtWindowCapture() override;

    auto Start(std::span<const DrmFormat> dmabuf_formats, FrameProcessor processor) -> void override;
    [[nodiscard]] auto IsStarted() const -> bool override { return m_channel != nullptr; }
    [[nodiscard]] auto SupportsDmabuf() const -> bool override { return false; }
    auto Receive() -> std::unique_ptr<ProcessedFrame> override;
    auto Pause() -> void override {}
    auto Resume() -> void override;
    auto RequestNewFrame() -> void override;

    ExtWindowCapture(const ExtWindowCapture&) = delete;
    auto operator=(const ExtWindowCapture&) -> ExtWindowCapture& = delete;

  private:
    using Channel = FrameChannel<std::unique_ptr<ProcessedFrame>>;

    static auto CaptureFrame(
        std::unique_ptr<ExtCaptureState> state,
        const std::shared_ptr<Channel>& channel,
        const FrameProcessor& processor,
        const std::shared_ptr<std::atomic<bool>>& stop
    ) -> std::unique_ptr<ExtCaptureState>;
    static auto Pump(ExtCaptureState& state, const std::atomic<bool>& stop, const std::function<bool()>& done) -> bool;
    static auto Release(ExtCaptureState& state) -> void;

    std::string m_name;
    std::unique_ptr<ExtCaptureState> m_state;
    std::future<std::unique_ptr<ExtCaptureState>> m_worker;
    std::shared_ptr<Channel> m_channel;
    std::shared_ptr<std::atomic<bool>> m_stop = std::make_shared<std::atomic<bool>>(false);
    FrameProcessor m_processor;
};
