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

#include "WgcCapture.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <utility>

#include <dxgi.h>
#include <roapi.h>
#include <windows.graphics.capture.interop.h>
#include <wrl/wrappers/corewrappers.h>

#include "DxgiInterfaceAccess.hpp"
#include "Logger.hpp"

extern "C" auto WINAPI CreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice* device, IInspectable** result) -> HRESULT;

using ABI::Windows::Foundation::IClosable;
using ABI::Windows::Graphics::SizeInt32;
using ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame;
using ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePoolStatics2;
using ABI::Windows::Graphics::Capture::IGraphicsCaptureSession2;
using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;

namespace
{

constexpr auto kPixelFormat = ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized;
constexpr INT32 kBuffers = 2;
constexpr auto kPollDelay = std::chrono::milliseconds(4);
constexpr auto kPausedDelay = std::chrono::milliseconds(50);

} // namespace

WgcCapture::WgcCapture(std::string name, HWND window) : m_name(std::move(name)), m_window(window), m_tracker(window)
{
}

WgcCapture::~WgcCapture()
{
    m_running.store(false);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

auto WgcCapture::Start(std::span<const DrmFormat>, FrameProcessor processor) -> void
{
    m_channel = std::make_shared<FrameChannel<std::unique_ptr<ProcessedFrame>>>(2);
    m_processor = std::move(processor);
    m_thread = std::thread(&WgcCapture::Run, this);
}

auto WgcCapture::Receive() -> std::unique_ptr<ProcessedFrame>
{
    if (!m_channel)
    {
        return nullptr;
    }
    auto frames = m_channel->TakeAll();
    if (frames.empty())
    {
        return nullptr;
    }
    return std::move(frames.back());
}

auto WgcCapture::Resume() -> void
{
    if (m_channel)
    {
        m_channel->TakeAll();
    }
    m_paused.store(false);
}

auto WgcCapture::Run() -> void
{
    const HRESULT initialized = RoInitialize(RO_INIT_MULTITHREADED);
    if (auto session = Begin())
    {
        Loop(*session);
        Close(*session);
    }
    else
    {
        LOG_ERROR("{}: {}", m_name, session.error().Message());
    }
    if (SUCCEEDED(initialized))
    {
        RoUninitialize();
    }
}

auto WgcCapture::Begin() const -> Result<WgcSession>
{
    WgcSession made;
    constexpr D3D_FEATURE_LEVEL kLevel = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            &kLevel,
            1,
            D3D11_SDK_VERSION,
            &made.device,
            nullptr,
            &made.context
        )))
    {
        return Unexpected("Could not create a D3D11 device");
    }
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IInspectable> inspectable;
    if (FAILED(made.device.As(&dxgi)) || FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), &inspectable))
        || FAILED(inspectable.As(&made.direct3d)))
    {
        return Unexpected("Could not share the D3D11 device with Windows Graphics Capture");
    }

    ComPtr<IGraphicsCaptureItemInterop> interop;
    if (FAILED(RoGetActivationFactory(
            HStringReference(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem).Get(),
            IID_PPV_ARGS(&interop)
        ))
        || FAILED(interop->CreateForWindow(m_window, IID_PPV_ARGS(&made.item))))
    {
        return Unexpected(std::format("Windows does not allow capturing {}", m_name));
    }
    made.item->get_Size(&made.size);

    ComPtr<IDirect3D11CaptureFramePoolStatics2> pools;
    if (FAILED(RoGetActivationFactory(
            HStringReference(RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool).Get(),
            IID_PPV_ARGS(&pools)
        ))
        || FAILED(pools->CreateFreeThreaded(made.direct3d.Get(), kPixelFormat, kBuffers, made.size, &made.pool))
        || FAILED(made.pool->CreateCaptureSession(made.item.Get(), &made.session)))
    {
        return Unexpected("Could not start Windows Graphics Capture");
    }
    if (ComPtr<IGraphicsCaptureSession2> cursor; SUCCEEDED(made.session.As(&cursor)))
    {
        cursor->put_IsCursorCaptureEnabled(TRUE);
    }
    if (FAILED(made.session->StartCapture()))
    {
        return Unexpected(std::format("Windows refused to capture {}", m_name));
    }
    return made;
}

auto WgcCapture::Loop(WgcSession& session) -> void
{
    std::vector<std::byte> pixels;
    while (m_running.load())
    {
        ComPtr<IDirect3D11CaptureFrame> frame;
        if (m_paused.load())
        {
            std::this_thread::sleep_for(kPausedDelay);
            continue;
        }
        if (FAILED(session.pool->TryGetNextFrame(&frame)) || !frame)
        {
            std::this_thread::sleep_for(kPollDelay);
            continue;
        }

        SizeInt32 content{};
        ComPtr<IDirect3DSurface> surface;
        ComPtr<IDirect3DDxgiInterfaceAccess> access;
        ComPtr<ID3D11Texture2D> texture;
        frame->get_ContentSize(&content);
        if (SUCCEEDED(frame->get_Surface(&surface)) && SUCCEEDED(surface.As(&access))
            && SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&texture))))
        {
            if (const auto format = Copy(session, *texture.Get(), content, pixels))
            {
                const MemPtrFrame captured{.format = *format, .data = pixels.data(), .size = pixels.size()};
                if (auto processed = m_processor(Frame(captured)))
                {
                    m_channel->Push(std::move(processed));
                }
            }
        }
        if (content.Width != session.size.Width || content.Height != session.size.Height)
        {
            session.size = content;
            session.pool->Recreate(session.direct3d.Get(), kPixelFormat, kBuffers, content);
        }
    }
}

auto WgcCapture::Copy(WgcSession& session, ID3D11Texture2D& texture, SizeInt32 content, std::vector<std::byte>& pixels)
    -> std::optional<FrameFormat>
{
    D3D11_TEXTURE2D_DESC source{};
    texture.GetDesc(&source);
    const auto width = std::min(static_cast<UINT>(std::max(content.Width, 1)), source.Width);
    const auto height = std::min(static_cast<UINT>(std::max(content.Height, 1)), source.Height);

    D3D11_TEXTURE2D_DESC staging{};
    if (session.staging)
    {
        session.staging->GetDesc(&staging);
    }
    if (!session.staging || staging.Width != width || staging.Height != height)
    {
        const D3D11_TEXTURE2D_DESC desc{
            .Width = width,
            .Height = height,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = source.Format,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .Usage = D3D11_USAGE_STAGING,
            .BindFlags = 0,
            .CPUAccessFlags = D3D11_CPU_ACCESS_READ,
            .MiscFlags = 0,
        };
        session.staging.Reset();
        if (FAILED(session.device->CreateTexture2D(&desc, nullptr, &session.staging)))
        {
            LOG_ERROR("Could not create a {}x{} staging texture", width, height);
            return std::nullopt;
        }
    }

    const D3D11_BOX box{.left = 0, .top = 0, .front = 0, .right = width, .bottom = height, .back = 1};
    session.context->CopySubresourceRegion(session.staging.Get(), 0, 0, 0, 0, &texture, 0, &box);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(session.context->Map(session.staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        return std::nullopt;
    }
    const size_t row = static_cast<size_t>(width) * 4;
    pixels.resize(row * height);
    const auto* source_bytes = static_cast<const std::byte*>(mapped.pData);
    for (UINT y = 0; y < height; ++y)
    {
        std::memcpy(pixels.data() + (y * row), source_bytes + (static_cast<size_t>(y) * mapped.RowPitch), row);
    }
    session.context->Unmap(session.staging.Get(), 0);
    return FrameFormat{
        .width = width,
        .height = height,
        .fourcc = kDrmFormatXrgb8888,
        .modifier = 0,
        .transform = FrameTransform::Normal
    };
}

auto WgcCapture::Close(WgcSession& session) -> void
{
    if (ComPtr<IClosable> closable; session.session && SUCCEEDED(session.session.As(&closable)))
    {
        closable->Close();
    }
    if (ComPtr<IClosable> closable; session.pool && SUCCEEDED(session.pool.As(&closable)))
    {
        closable->Close();
    }
}
