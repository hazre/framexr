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
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include "Capture.hpp"
#include "Error.hpp"
#include "FrameChannel.hpp"
#include "GraphicsCapture.hpp"
#include "HwndTracker.hpp"

struct WgcSession
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> direct3d;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> item;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool> pool;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession> session;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    ABI::Windows::Graphics::SizeInt32 size{};
};

class WgcCapture final : public Capture
{
  public:
    WgcCapture(std::string name, HWND window);
    ~WgcCapture() override;
    WgcCapture(const WgcCapture&) = delete;
    auto operator=(const WgcCapture&) -> WgcCapture& = delete;

    auto Start(std::span<const DrmFormat> dmabuf_formats, FrameProcessor processor) -> void override;
    [[nodiscard]] auto IsStarted() const -> bool override { return m_channel != nullptr; }
    [[nodiscard]] auto SupportsDmabuf() const -> bool override { return false; }
    auto Receive() -> std::unique_ptr<ProcessedFrame> override;
    auto Pause() -> void override { m_paused.store(true); }
    auto Resume() -> void override;
    auto RequestNewFrame() -> void override {}
    auto Window() -> WindowTracker* override { return &m_tracker; }

  private:
    auto Run() -> void;
    [[nodiscard]] auto Begin() const -> Result<WgcSession>;
    auto Loop(WgcSession& session) -> void;
    static auto Copy(
        WgcSession& session,
        ID3D11Texture2D& texture,
        ABI::Windows::Graphics::SizeInt32 content,
        std::vector<std::byte>& pixels
    ) -> std::optional<FrameFormat>;
    static auto Close(WgcSession& session) -> void;

    std::string m_name;
    HWND m_window;
    HwndTracker m_tracker;
    FrameProcessor m_processor;
    std::shared_ptr<FrameChannel<std::unique_ptr<ProcessedFrame>>> m_channel;
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_paused{false};
    std::thread m_thread;
};
