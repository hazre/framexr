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

#include "WindowToolbar.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "AppPicker.hpp"
#include "ButtonComponent.hpp"
#include "Compose.hpp"
#include "Container.hpp"
#include "Desktop.hpp"
#include "Form.hpp"
#include "FormTheme.hpp"
#include "Mirror.hpp"
#include "PanelComponent.hpp"
#include "RowComponent.hpp"
#include "TaskQueue.hpp"

namespace
{

constexpr float kButtonWidth = 84.0f;
constexpr float kHeight = 40.0f;
constexpr float kPadding = 4.0f;
constexpr float kGap = 4.0f;
constexpr int kFontSize = 14;
constexpr float kMetersPerPixel = 1.0f / 3200.0f;
constexpr float kMarginMeters = 0.01f;
constexpr auto kLinger = std::chrono::milliseconds(800);

} // namespace

auto WindowToolbar::Create() -> Result<std::unique_ptr<WindowToolbar>>
{
    auto toolbar = std::unique_ptr<WindowToolbar>(new WindowToolbar());
    toolbar->SetName(std::string(kWindowToolbarName));
    toolbar->SetInteractable(true);
    toolbar->SetZOrder(kZOrderWatch);
    RETURN_IF_ERROR(toolbar->Build(false));
    return toolbar;
}

auto WindowToolbar::Build(bool app) -> Result<>
{
    std::vector<std::pair<std::string, std::function<void()>>> items{
        {"Lock", [this]() -> void { Act(OverlayToggleImmovable{}); }},
        {"Opacity",
         [this]() -> void
         {
             if (m_target)
             {
                 g_tasks->Enqueue(
                     ModifyOverlayTask{
                         .selector = *m_target,
                         .modify = [](View& overlay) -> void
                         {
                             overlay.SetAlpha(overlay.Alpha() <= 0.3f ? 1.0f : overlay.Alpha() - 0.25f);
                             overlay.SetDirty(true);
                         },
                     }
                 );
             }
         }},
    };
    if (app)
    {
        items.push_back(
            {"Switch",
             [this]() -> void
             {
                 if (const auto* target = m_target ? g_container->ById(*m_target) : nullptr)
                 {
                     AppPicker::Open(target->Name());
                 }
             }}
        );
        items.push_back(
            {"Close",
             [this]() -> void
             {
                 if (m_target)
                 {
                     g_tasks->Enqueue(DropOverlayTask{.selector = *m_target});
                 }
             }}
        );
    }
    else
    {
        items.push_back({"Hide", [this]() -> void { Act(OverlayToggleVisible{}); }});
    }

    m_size = {kButtonWidth * static_cast<float>(items.size()) + kPadding * 2.0f, kHeight};
    auto form = std::make_unique<Form>(std::string(kWindowToolbarName));
    form->SetSize(m_size);
    auto& background = form->AddComponent<PanelComponent>("background");
    background.SetRect({0.0f, 0.0f, m_size.x(), m_size.y()});
    background.SetCornerRadius(kHeight / 2.0f);
    background.SetBgColor(FormTheme::Rgba("@panel"));
    auto& row = form->AddComponent<RowComponent>("buttons", kHeight - kPadding * 2.0f, kGap);
    row.SetRect({kPadding, kPadding, m_size.x() - kPadding * 2.0f, kHeight - kPadding * 2.0f});
    for (auto& [text, run] : items)
    {
        auto& button = row.AddComponent<ButtonComponent>(text, text);
        button.SetFontSize(kFontSize);
        button.SetCornerRadius((kHeight - kPadding * 2.0f) / 2.0f);
        button.SetFgColor(FormTheme::Rgba("@text"));
        button.SetBgColor(FormTheme::Rgba("@surface"));
        button.SetOnRelease(std::move(run));
    }

    ASSIGN_OR_RETURN(auto composer, Compose::Create(std::move(form)));
    SetComposer(std::move(composer));
    SetInteractionTransform(UiTransform({static_cast<uint32_t>(m_size.x()), static_cast<uint32_t>(m_size.y())}));
    m_app = app;
    return {};
}

auto WindowToolbar::Update() -> void
{
    auto* target = FindTarget();
    if (target == nullptr || !Place(*target))
    {
        m_target.reset();
        SetWantVisible(false);
        return;
    }
    m_target = target->Id();
    const bool app = dynamic_cast<const Mirror*>(target) != nullptr;
    if (m_app != app)
    {
        if (const auto built = Build(app); !built)
        {
            SetWantVisible(false);
            return;
        }
        Place(*target);
    }
    SetWantVisible(true);
}

auto WindowToolbar::FindTarget() -> View*
{
    const auto now = std::chrono::steady_clock::now();
    if (m_target && now - HoveredAt() < kLinger)
    {
        if (auto* current = g_container->ById(*m_target); current != nullptr && current->WantVisible())
        {
            return current;
        }
    }
    std::vector<View*> windows;
    std::ranges::copy(g_container->Desktops(), std::back_inserter(windows));
    std::ranges::copy(g_container->Apps(), std::back_inserter(windows));
    std::erase_if(
        windows,
        [now](const View* window) -> bool { return !window->WantVisible() || now - window->HoveredAt() >= kLinger; }
    );
    const auto latest = std::ranges::max_element(windows, {}, &View::HoveredAt);
    return latest == windows.end() ? nullptr : *latest;
}

auto WindowToolbar::Place(View& target) -> bool
{
    auto* composer = target.Composer();
    const auto meta = composer != nullptr ? composer->Meta() : std::nullopt;
    if (!meta || meta->extent.width == 0)
    {
        return false;
    }
    const auto parent = target.ParentTransform();
    const Eigen::Affine3f base = parent ? target.LocalTransform() : target.Transform();
    const Eigen::Matrix3f linear = base.linear();
    const float width = linear.col(0).norm();
    const Eigen::Matrix3f facing = linear / width;
    const float aspect = static_cast<float>(meta->extent.width) / static_cast<float>(meta->extent.height);
    const float height = width * std::min(aspect, 1.0f) / aspect;
    const float window_width = width * std::min(aspect, 1.0f);
    const float toolbar_width = std::min(m_size.x() * kMetersPerPixel, window_width);
    const float toolbar_height = toolbar_width * m_size.y() / m_size.x();

    Eigen::Affine3f pose = Eigen::Affine3f::Identity();
    pose.translation() = base.translation()
                       + facing * Eigen::Vector3f(0.0f, -(height + toolbar_height) / 2.0f - kMarginMeters, 0.0f);
    pose.linear() = facing * toolbar_width;
    SetCurvature(target.Curvature().transform([&](float curvature) -> float { return curvature * toolbar_width / window_width; }));
    SetAttachedTo(parent ? target.AttachedTo() : RelativeTo::Stage);
    SetSavedTransform(parent ? std::optional(pose) : std::nullopt);
    SetTransform(parent ? *parent * pose : pose);
    SetDirty(true);
    return true;
}

auto WindowToolbar::Act(const OverlayAction& action) -> void
{
    if (m_target)
    {
        m_runner.Run(OverlayButtonAction{.target = *m_target, .action = action});
    }
}
