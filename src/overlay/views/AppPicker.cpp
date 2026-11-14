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

#include "AppPicker.hpp"

#include <format>
#include <utility>
#include <vector>

#include "ButtonComponent.hpp"
#include "ChoiceGridComponent.hpp"
#include "Compose.hpp"
#include "Container.hpp"
#include "Form.hpp"
#include "FormTheme.hpp"
#include "Mirror.hpp"
#include "StackComponent.hpp"
#include "TaskQueue.hpp"
#include "TextLineComponent.hpp"
#include "Toast.hpp"
#include "WindowSource.hpp"
#if FRAMEXR_FEATURE_WAYLAND
#include "ScreenCastPortal.hpp"
#endif

namespace
{

constexpr float kWidth = 420.0f;
constexpr float kPadding = 12.0f;
constexpr float kGap = 6.0f;
constexpr float kRowHeight = 30.0f;
constexpr float kButtonHeight = 34.0f;
constexpr float kTitleHeight = 24.0f;
constexpr int kTitleFont = 18;
constexpr int kTextFont = 14;
constexpr float kWidthMeters = 0.4f;
constexpr size_t kMaxWindows = 24;

} // namespace

auto AppPicker::Open(std::optional<std::string> replace) -> void
{
    g_tasks->Enqueue(DropOverlayTask{.selector = std::string(kAppPickerName)});
    g_tasks->Enqueue(
        CreateOverlayTask{
            .selector = std::string(kAppPickerName),
            .create = [replace = std::move(replace)]() -> std::unique_ptr<View> { return Create(replace); },
        }
    );
}

auto AppPicker::Create(const std::optional<std::string>& replace) -> std::unique_ptr<View>
{
    const auto source = WindowSource::Create();
    if (!source)
    {
#if FRAMEXR_FEATURE_WAYLAND
        const auto name = replace.value_or(UniqueName("App"));
        Toast::Show(ToastMessage{.topic = ToastTopic::System, .title = "Check your desktop for popup.", .sound = true});
        ShowApp(name, ScreenCastPortal::SelectWindow(name));
#else
        Toast::ShowError("App capture is not supported on this desktop");
#endif
        return nullptr;
    }
    auto windows = source->Windows();
    if (!windows)
    {
        Toast::ShowError(windows.error().Message());
        return nullptr;
    }
    if (windows->size() > kMaxWindows)
    {
        windows->resize(kMaxWindows);
    }

    auto form = std::make_unique<Form>(std::string(kAppPickerName));
    auto& stack = form->AddComponent<StackComponent>("stack", kPadding, kGap);
    stack.SetBgColor(FormTheme::Rgba("@panel"));
    stack.AddComponent<TextLineComponent>(
        "title",
        windows->empty() ? "No apps to share" : "Choose an app to share",
        kTitleFont,
        FormTheme::Rgba("@text"),
        kTitleHeight
    );

    std::vector<std::string> titles;
    titles.reserve(windows->size());
    for (const auto& window : *windows)
    {
        titles.push_back(window.title);
    }
    auto& choices = stack.AddComponent<ChoiceGridComponent>("apps", titles, std::nullopt, 1, kRowHeight);
    choices.SetOnChoose(
        [source, windows = std::move(*windows), replace](size_t index) -> void
        {
            const auto& window = windows.at(index);
            ShowApp(replace.value_or(UniqueName(window.title)), source->Open(window));
            g_tasks->Enqueue(DropOverlayTask{.selector = std::string(kAppPickerName)});
        }
    );

    auto& cancel = stack.AddComponent<ButtonComponent>("cancel", "Cancel");
    cancel.SetSize({0.0f, kButtonHeight});
    cancel.SetFontSize(kTextFont);
    cancel.SetFgColor(FormTheme::Rgba("@text"));
    cancel.SetBgColor(FormTheme::Rgba("@surface"));
    cancel.SetOnRelease([]() -> void { g_tasks->Enqueue(DropOverlayTask{.selector = std::string(kAppPickerName)}); });

    const Eigen::Vector2f size(kWidth, stack.Bottom() - kGap + kPadding);
    form->SetSize(size);
    stack.SetSize(size);

    auto created = Compose::Create(std::move(form));
    if (!created)
    {
        return nullptr;
    }

    auto picker = std::unique_ptr<AppPicker>(new AppPicker());
    picker->SetComposer(std::move(*created));
    picker->SetName(std::string(kAppPickerName));
    picker->SetWantVisible(true);
    picker->SetInteractable(true);
    picker->SetGrabbable(true);
    picker->SetZOrder(kZOrderToast);
    picker->SetSpawnScale(kWidthMeters);
    picker->SetSpawnPoint(Eigen::Vector3f(0.0f, -0.05f, -0.5f));
    picker->SetInteractionTransform(UiTransform({static_cast<uint32_t>(size.x()), static_cast<uint32_t>(size.y())}));
    return picker;
}

auto AppPicker::ShowApp(std::string name, std::future<Result<std::unique_ptr<Capture>>> capture) -> void
{
    auto pending = std::make_shared<std::future<Result<std::unique_ptr<Capture>>>>(std::move(capture));
    if (dynamic_cast<Mirror*>(g_container->ByName(name)) != nullptr)
    {
        g_tasks->Enqueue(
            ModifyOverlayTask{
                .selector = name,
                .modify = [pending](View& overlay) -> void { static_cast<Mirror&>(overlay).Switch(std::move(*pending)); },
            }
        );
        return;
    }
    g_tasks->Enqueue(
        CreateOverlayTask{
            .selector = name,
            .create = [name, pending]() -> std::unique_ptr<View> { return std::make_unique<Mirror>(name, std::move(*pending)); },
        }
    );
}

auto AppPicker::UniqueName(const std::string& title) -> std::string
{
    auto name = title;
    for (int index = 2; g_container->ByName(name) != nullptr; ++index)
    {
        name = std::format("{} ({})", title, index);
    }
    return name;
}
