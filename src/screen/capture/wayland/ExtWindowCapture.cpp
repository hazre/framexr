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

#include "ExtWindowCapture.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>

#include <wayland-client.h>

#include "ExtWindowSource.hpp"
#include "Logger.hpp"
#include "ShmBuffer.hpp"
#include "WaylandClient.hpp"
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"

namespace
{

constexpr auto kSetupTimeout = std::chrono::seconds(5);
constexpr auto kPollInterval = std::chrono::milliseconds(100);
constexpr uint32_t kBytesPerPixel = 4;

constexpr ext_image_copy_capture_session_v1_listener kSessionListener{
    .buffer_size = [](void* data, ext_image_copy_capture_session_v1*, uint32_t width, uint32_t height) -> void
    {
        auto* state = static_cast<ExtCaptureState*>(data);
        state->width = width;
        state->height = height;
        state->shm_format.reset();
        state->constrained = false;
    },
    .shm_format = [](void* data, ext_image_copy_capture_session_v1*, uint32_t format) -> void
    {
        auto* state = static_cast<ExtCaptureState*>(data);
        if (!state->shm_format && ShmBuffer::Fourcc(format))
        {
            state->shm_format = format;
        }
    },
    .dmabuf_device = [](void*, ext_image_copy_capture_session_v1*, wl_array*) -> void {},
    .dmabuf_format = [](void*, ext_image_copy_capture_session_v1*, uint32_t, wl_array*) -> void {},
    .done = [](void* data, ext_image_copy_capture_session_v1*) -> void { static_cast<ExtCaptureState*>(data)->constrained = true; },
    .stopped = [](void* data, ext_image_copy_capture_session_v1*) -> void { static_cast<ExtCaptureState*>(data)->stopped = true; },
};

constexpr ext_image_copy_capture_frame_v1_listener kFrameListener{
    .transform = [](void* data, ext_image_copy_capture_frame_v1*, uint32_t transform) -> void
    { static_cast<ExtFrameRequest*>(data)->transform = WaylandClient::ToFrameTransform(static_cast<int32_t>(transform)); },
    .damage = [](void*, ext_image_copy_capture_frame_v1*, int32_t, int32_t, int32_t, int32_t) -> void {},
    .presentation_time = [](void*, ext_image_copy_capture_frame_v1*, uint32_t, uint32_t, uint32_t) -> void {},
    .ready = [](void* data, ext_image_copy_capture_frame_v1*) -> void
    {
        auto* request = static_cast<ExtFrameRequest*>(data);
        request->ready = true;
        request->finished = true;
    },
    .failed = [](void* data, ext_image_copy_capture_frame_v1*, uint32_t reason) -> void
    {
        auto* request = static_cast<ExtFrameRequest*>(data);
        request->failure = reason;
        request->finished = true;
    },
};

} // namespace

auto ExtWindowCapture::Open(const AppWindow& window) -> Result<std::unique_ptr<Capture>>
{
    ASSIGN_OR_RETURN(auto connection, WaylandConnection::Connect());
    auto state = std::make_unique<ExtCaptureState>();
    state->connection = std::move(connection);
    auto* setup = state.get();
    auto capture = std::make_unique<ExtWindowCapture>(window.title, std::move(state));

    RETURN_IF_ERROR(ExtWindowSource::List(*setup->connection, setup->list));
    const auto toplevel = std::ranges::find_if(
        setup->list.toplevels,
        [&](const ExtToplevel& candidate) -> bool { return !candidate.closed && candidate.window.id == window.id; }
    );
    if (toplevel == setup->list.toplevels.end())
    {
        return Unexpected(std::format("{} is no longer open", window.title));
    }
    setup->shm = setup->connection->Bind<wl_shm>(wl_shm_interface, 1);
    setup->sources = setup->connection->Bind<ext_foreign_toplevel_image_capture_source_manager_v1>(
        ext_foreign_toplevel_image_capture_source_manager_v1_interface,
        1
    );
    setup->manager = setup->connection->Bind<ext_image_copy_capture_manager_v1>(ext_image_copy_capture_manager_v1_interface, 1);
    if (setup->shm == nullptr || setup->sources == nullptr || setup->manager == nullptr)
    {
        return Unexpected("The compositor does not allow FrameXR to capture windows");
    }

    setup->source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(setup->sources, toplevel->proxy);
    setup->session = ext_image_copy_capture_manager_v1_create_session(
        setup->manager,
        setup->source,
        EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS
    );
    ext_image_copy_capture_session_v1_add_listener(setup->session, &kSessionListener, setup);
    while (!setup->constrained && !setup->stopped)
    {
        ASSIGN_OR_RETURN(const bool read, setup->connection->Dispatch(kSetupTimeout));
        if (!read)
        {
            return Unexpected("Timed out waiting for the compositor");
        }
    }
    if (setup->stopped || !setup->shm_format)
    {
        return Unexpected(std::format("The compositor refused to capture {}", window.title));
    }
    return capture;
}

ExtWindowCapture::ExtWindowCapture(std::string name, std::unique_ptr<ExtCaptureState> state)
    : m_name(std::move(name)),
      m_state(std::move(state))
{
}

ExtWindowCapture::~ExtWindowCapture()
{
    m_stop->store(true);
    if (m_worker.valid())
    {
        m_state = m_worker.get();
    }
    if (m_state)
    {
        Release(*m_state);
    }
}

auto ExtWindowCapture::Start(std::span<const DrmFormat>, FrameProcessor processor) -> void
{
    m_channel = std::make_shared<Channel>(2);
    m_processor = std::move(processor);
}

auto ExtWindowCapture::Receive() -> std::unique_ptr<ProcessedFrame>
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

auto ExtWindowCapture::Resume() -> void
{
    if (m_channel)
    {
        Receive();
        RequestNewFrame();
    }
}

auto ExtWindowCapture::RequestNewFrame() -> void
{
    if (m_worker.valid())
    {
        if (m_worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return;
        }
        m_state = m_worker.get();
    }
    if (!m_state || !m_channel || m_state->stopped)
    {
        return;
    }
    m_worker = std::async(std::launch::async, &ExtWindowCapture::CaptureFrame, std::move(m_state), m_channel, m_processor, m_stop);
}

auto ExtWindowCapture::CaptureFrame(
    std::unique_ptr<ExtCaptureState> state,
    const std::shared_ptr<Channel>& channel,
    const FrameProcessor& processor,
    const std::shared_ptr<std::atomic<bool>>& stop
) -> std::unique_ptr<ExtCaptureState>
{
    auto& current = *state;
    if (!Pump(current, *stop, [&current]() -> bool { return current.constrained; }))
    {
        return state;
    }
    const auto format = current.shm_format;
    if (!format)
    {
        return state;
    }
    const uint32_t shm_format = *format;
    const uint32_t stride = current.width * kBytesPerPixel;
    const auto buffer = ShmBuffer::Create(current.shm, current.width, current.height, stride, shm_format);
    if (!buffer)
    {
        return state;
    }

    ExtFrameRequest request;
    auto* frame = ext_image_copy_capture_session_v1_create_frame(current.session);
    ext_image_copy_capture_frame_v1_add_listener(frame, &kFrameListener, &request);
    ext_image_copy_capture_frame_v1_attach_buffer(frame, buffer->Buffer());
    ext_image_copy_capture_frame_v1_damage_buffer(
        frame,
        0,
        0,
        static_cast<int32_t>(current.width),
        static_cast<int32_t>(current.height)
    );
    ext_image_copy_capture_frame_v1_capture(frame);
    Pump(current, *stop, [&request]() -> bool { return request.finished; });
    ext_image_copy_capture_frame_v1_destroy(frame);

    if (request.failure == EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED)
    {
        current.stopped = true;
    }
    if (request.ready)
    {
        const MemFdFrame captured{
            .format =
                {
                    .width = current.width,
                    .height = current.height,
                    .fourcc = ShmBuffer::Fourcc(shm_format).value_or(kDrmFormatArgb8888),
                    .transform = request.transform,
                },
            .plane = {.fd = buffer->Fd(), .offset = 0, .stride = static_cast<int32_t>(stride)},
        };
        if (auto processed = processor(Frame(captured)))
        {
            channel->Push(std::move(processed));
        }
    }
    return state;
}

auto ExtWindowCapture::Pump(ExtCaptureState& state, const std::atomic<bool>& stop, const std::function<bool()>& done) -> bool
{
    while (!done() && !state.stopped && !stop.load())
    {
        if (const auto read = state.connection->Dispatch(kPollInterval); !read)
        {
            LOG_WARN("Window capture lost the compositor: {}", read.error().Message());
            state.stopped = true;
        }
    }
    return done();
}

auto ExtWindowCapture::Release(ExtCaptureState& state) -> void
{
    if (state.session != nullptr)
    {
        ext_image_copy_capture_session_v1_destroy(state.session);
    }
    if (state.source != nullptr)
    {
        ext_image_capture_source_v1_destroy(state.source);
    }
    if (state.manager != nullptr)
    {
        ext_image_copy_capture_manager_v1_destroy(state.manager);
    }
    if (state.sources != nullptr)
    {
        ext_foreign_toplevel_image_capture_source_manager_v1_destroy(state.sources);
    }
    if (state.shm != nullptr)
    {
        wl_shm_destroy(state.shm);
    }
    ExtWindowSource::Release(state.list);
    wl_display_flush(state.connection->Display());
}
