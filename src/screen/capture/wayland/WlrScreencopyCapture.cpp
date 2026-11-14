// FrameXR
//
// This is a port of wlx-capture from Rust to C++.
//
// Copyright (C) 2026 Nyabsi <nyabsi@sovellus.cc>
// Copyright (C) 2023-2025 galister and wlx-capture contributors
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

#include "WlrScreencopyCapture.hpp"

#include <chrono>
#include <utility>

#include "Error.hpp"
#include "Logger.hpp"
#include "ShmBuffer.hpp"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

WlrScreencopyCapture::WlrScreencopyCapture(std::unique_ptr<WaylandClient> client, uint32_t output_id)
    : m_output_id(output_id),
      m_client(std::move(client))
{
}

WlrScreencopyCapture::~WlrScreencopyCapture()
{
    if (m_worker.valid())
    {
        m_worker.wait();
    }
}

auto WlrScreencopyCapture::Start(std::span<const DrmFormat>, FrameProcessor processor) -> void
{
    m_channel = std::make_shared<ProcessedFrameChannel>(2);
    m_processor = std::move(processor);
}

auto WlrScreencopyCapture::Receive() -> std::unique_ptr<ProcessedFrame>
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

auto WlrScreencopyCapture::Resume() -> void
{
    if (!m_channel)
    {
        return;
    }
    Receive();
    RequestNewFrame();
}

auto WlrScreencopyCapture::RequestNewFrame() -> void
{
    bool wait_for_damage = false;
    if (m_worker.valid())
    {
        if (m_worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return;
        }
        wait_for_damage = true;
        m_client = m_worker.get();
    }
    if (!m_client)
    {
        return;
    }
    if (!m_channel)
    {
        FatalError("must call Start once before RequestNewFrame");
    }
    m_worker = std::async(
        std::launch::async,
        &WlrScreencopyCapture::CaptureFrame,
        std::move(m_client),
        m_output_id,
        m_channel,
        m_processor,
        wait_for_damage
    );
}

auto WlrScreencopyCapture::CaptureFrame(
    std::unique_ptr<WaylandClient> client,
    uint32_t output_id,
    const std::shared_ptr<ProcessedFrameChannel>& channel,
    const FrameProcessor& processor,
    bool wait_for_damage
) -> std::unique_ptr<WaylandClient>
{
    static constexpr zwlr_screencopy_frame_v1_listener kListener{
        .buffer = &WlrScreencopyCapture::OnBuffer,
        .flags = &WlrScreencopyCapture::OnFlags,
        .ready = &WlrScreencopyCapture::OnReady,
        .failed = &WlrScreencopyCapture::OnFailed,
        .damage = &WlrScreencopyCapture::OnDamage,
        .linux_dmabuf = &WlrScreencopyCapture::OnLinuxDmabuf,
        .buffer_done = &WlrScreencopyCapture::OnBufferDone,
    };

    auto* manager = client->ScreencopyManager();
    const auto* output = client->FindOutput(output_id);
    if (manager == nullptr || output == nullptr)
    {
        return client;
    }

    const std::string name = output->name;
    ScreencopyRequest request{
        .client = client.get(),
        .name = name,
        .transform = WaylandClient::ToFrameTransform(output->transform),
        .wait_for_damage = wait_for_damage,
    };
    request.frame = zwlr_screencopy_manager_v1_capture_output(manager, 1, output->output);
    zwlr_screencopy_frame_v1_add_listener(request.frame, &kListener, &request);

    while (!request.finished)
    {
        if (wl_display_dispatch(client->Display()) < 0)
        {
            zwlr_screencopy_frame_v1_destroy(request.frame);
            return client;
        }
    }

    if (request.ready && request.buffer)
    {
        if (auto result = processor(Frame(request.mem_frame)))
        {
            channel->Push(std::move(result));
            LOG_TRACE("{}: Frame ready", name);
        }
    }
    else if (!request.ready)
    {
        LOG_TRACE("{}: Frame failed", name);
    }
    return client;
}

auto WlrScreencopyCapture::OnBuffer(
    void* data,
    zwlr_screencopy_frame_v1* frame,
    uint32_t format,
    uint32_t width,
    uint32_t height,
    uint32_t stride
) -> void
{
    auto* request = static_cast<ScreencopyRequest*>(data);
    if (request->buffer)
    {
        return;
    }

    const auto fourcc = ShmBuffer::Fourcc(format);
    if (!fourcc)
    {
        LOG_WARN("Unsupported screencopy format");
        zwlr_screencopy_frame_v1_destroy(frame);
        request->finished = true;
        return;
    }

    request->buffer = ShmBuffer::Create(request->client->Shm(), width, height, stride, format);
    if (!request->buffer)
    {
        zwlr_screencopy_frame_v1_destroy(frame);
        request->finished = true;
        return;
    }
    request->mem_frame = MemFdFrame{
        .format = {.width = width, .height = height, .fourcc = *fourcc, .transform = request->transform},
        .plane = {.fd = request->buffer->Fd(), .offset = 0, .stride = static_cast<int32_t>(stride)},
    };
    LOG_TRACE("{}: Received screencopy buffer, copying", request->name);

    if (request->wait_for_damage)
    {
        zwlr_screencopy_frame_v1_copy_with_damage(frame, request->buffer->Buffer());
    }
    else
    {
        zwlr_screencopy_frame_v1_copy(frame, request->buffer->Buffer());
    }
}

auto WlrScreencopyCapture::OnFlags(void*, zwlr_screencopy_frame_v1*, uint32_t) -> void
{
}

auto WlrScreencopyCapture::OnReady(void* data, zwlr_screencopy_frame_v1* frame, uint32_t, uint32_t, uint32_t) -> void
{
    auto* request = static_cast<ScreencopyRequest*>(data);
    zwlr_screencopy_frame_v1_destroy(frame);
    request->ready = true;
    request->finished = true;
}

auto WlrScreencopyCapture::OnFailed(void* data, zwlr_screencopy_frame_v1* frame) -> void
{
    auto* request = static_cast<ScreencopyRequest*>(data);
    zwlr_screencopy_frame_v1_destroy(frame);
    request->finished = true;
}

auto WlrScreencopyCapture::OnDamage(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t, uint32_t) -> void
{
}

auto WlrScreencopyCapture::OnLinuxDmabuf(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t) -> void
{
}

auto WlrScreencopyCapture::OnBufferDone(void*, zwlr_screencopy_frame_v1*) -> void
{
}
