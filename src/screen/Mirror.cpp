// FrameXR
//
// This is a port of wlx-overlay-s from Rust to C++.
//
// Copyright (C) 2026 Nyabsi <nyabsi@sovellus.cc>
// Copyright (C) 2023-2025 galister and wlx-overlay-s contributors
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

#include "Mirror.hpp"

#include <chrono>
#include <utility>

#include "Compose.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "ScreenRenderer.hpp"
#include "TaskQueue.hpp"

Mirror::Mirror(std::string name, std::future<Result<std::unique_ptr<Capture>>> capture) : m_capture(std::move(capture))
{
    SetName(std::move(name));
    SetWantVisible(true);
    SetRecenter(true);
    SetShowHide(true);
    SetSpawnScale(0.5f * g_config->desktop_view_scale);
}

auto Mirror::Update() -> void
{
    if (m_capture && m_capture->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        auto capture = m_capture->get();
        m_capture.reset();
        Attach(std::move(capture));
    }
    if (const auto meta = Composer()->Meta();
        meta && (meta->extent.width != m_extent.width || meta->extent.height != m_extent.height))
    {
        m_extent = meta->extent;
        SetInteractionTransform(UiTransform(m_extent));
    }
}

auto Mirror::Switch(std::future<Result<std::unique_ptr<Capture>>> capture) -> void
{
    m_capture = std::move(capture);
}

auto Mirror::Attach(Result<std::unique_ptr<Capture>> capture) -> void
{
    if (!capture)
    {
        LOG_WARN("Failed to create mirror: {}", capture.error().Message());
        if (!m_attached)
        {
            g_tasks->Enqueue(DropOverlayTask{.selector = Name()});
        }
        return;
    }
    m_window = (*capture)->Window();
    m_input.reset();
    SetComposer(std::make_unique<Compose>(std::make_unique<ScreenRenderer>(Name(), std::move(*capture))));
    if (!std::exchange(m_attached, true))
    {
        SetGrabbable(true);
        SetInteractable(true);
        Reset(false);
    }
}

auto Mirror::OnHover(const PointerHit& hit) -> std::optional<Haptics>
{
    if (auto* input = Input())
    {
        input->Hover(hit);
    }
    return std::nullopt;
}

auto Mirror::OnPointer(const PointerHit& hit, bool pressed) -> void
{
    if (auto* input = Input())
    {
        if (pressed)
        {
            m_window->Activate();
        }
        input->Press(hit, pressed);
    }
}

auto Mirror::OnScroll(const PointerHit&, float delta_y, float delta_x) -> void
{
    if (Input() != nullptr)
    {
        ScreenInteraction::Scroll(delta_y, delta_x);
    }
}

auto Mirror::Input() -> ScreenInteraction*
{
    const auto bounds = m_window != nullptr ? m_window->Bounds() : std::nullopt;
    if (!bounds)
    {
        return nullptr;
    }
    if (!m_input || !bounds->isApprox(m_bounds))
    {
        m_bounds = *bounds;
        m_input.emplace(m_bounds.min(), m_bounds.sizes(), FrameTransform::Normal);
    }
    return &*m_input;
}
