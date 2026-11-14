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

#include "BatteryComponent.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <utility>

#include "DeviceModels.hpp"
#include "Devices.hpp"
#include "Json.hpp"
#include "document/FormParser.hpp"

namespace
{

constexpr std::array<std::string_view, 5> kDeviceTags{"", "H", "L", "R", "T"};
constexpr auto kStyleNames = std::to_array<std::pair<BatteryStyle, std::string_view>>({
    {BatteryStyle::Text, "Text"},
    {BatteryStyle::Model, "Model"},
});
constexpr float kBarShare = 0.1f;
constexpr float kMinBar = 3.0f;
constexpr float kMaxBar = 8.0f;
constexpr float kTrackAlpha = 0.25f;

} // namespace

auto BatteryComponent::Parse(const nlohmann::json& json) -> BatteryElement
{
    return {
        .font_size = json.at("font_size").get<int>(),
        .fg_color = json.at("fg_color").get<std::string>(),
        .centered = FormParser::Optional<bool>(json, "centered").value_or(false),
        .device = json.at("device").get<size_t>(),
        .low_threshold = json.at("low_threshold").get<float>(),
        .low_color = json.at("low_color").get<std::string>(),
        .charging_color = json.at("charging_color").get<std::string>(),
        .style = ParseStyle(json),
    };
}

auto BatteryComponent::Serialize(nlohmann::ordered_json& json, const BatteryElement& element) -> void
{
    json["font_size"] = element.font_size;
    json["fg_color"] = element.fg_color;
    if (element.centered)
    {
        json["centered"] = true;
    }
    json["device"] = element.device;
    json["low_threshold"] = Json::ShortestFloat(element.low_threshold);
    json["low_color"] = element.low_color;
    json["charging_color"] = element.charging_color;
    PutStyle(json, element.style);
}

auto BatteryComponent::Create(FormComponent& parent, const BatteryElement& element, const FormContext& context) -> BatteryComponent&
{
    auto& component = parent.AddComponent<BatteryComponent>("", element.device, element.low_threshold, element.style);
    component.SetRect(FormParser::ToRect(element.rect));
    component.Label().SetFontSize(element.font_size);
    component.Label().SetFgColor(FormParser::Color(element.fg_color, context.theme));
    component.Label().SetCentered(element.centered);
    component.SetColors(
        FormParser::Color(element.fg_color, context.theme),
        FormParser::Color(element.low_color, context.theme),
        FormParser::Color(element.charging_color, context.theme)
    );
    return component;
}

auto BatteryComponent::ParseStyle(const nlohmann::json& json) -> BatteryStyle
{
    const auto it = json.find("style");
    return it == json.end() || it->is_null() ? BatteryStyle::Text : FormParser::ParseName(*it, kStyleNames, "BatteryStyle");
}

auto BatteryComponent::PutStyle(nlohmann::ordered_json& json, BatteryStyle style) -> void
{
    if (style != BatteryStyle::Text)
    {
        json["style"] = FormParser::NameOf(style, kStyleNames);
    }
}

BatteryComponent::BatteryComponent(std::string name, size_t device, float low_threshold, BatteryStyle style)
    : FormComponent(std::move(name)),
      m_label(AddComponent<LabelComponent>("label")),
      m_device(device),
      m_low_threshold(low_threshold)
{
    if (style == BatteryStyle::Model)
    {
        m_sprite = &AddComponent<SpriteComponent>("model");
        m_track = &AddComponent<PanelComponent>("track");
        m_fill = &AddComponent<PanelComponent>("fill");
        m_track->SetBgColor(Eigen::Vector4f::Zero());
        m_fill->SetBgColor(Eigen::Vector4f::Zero());
    }
}

auto BatteryComponent::SetColors(const Eigen::Vector4f& normal, const Eigen::Vector4f& low, const Eigen::Vector4f& charging) -> void
{
    m_normal_color = normal;
    m_low_color = low;
    m_charging_color = charging;
    m_label.SetFgColor(normal);
}

auto BatteryComponent::Layout() -> void
{
    if (m_sprite == nullptr)
    {
        m_label.SetSize(Size());
        return;
    }
    const float bar = std::clamp(Size().y() * kBarShare, kMinBar, kMaxBar);
    const float side = std::max(std::min(Size().x(), Size().y() - (2.0f * bar)), 0.0f);
    const float left = 0.5f * (Size().x() - side);
    m_label.SetRect({0.0f, 0.0f, Size().x(), side});
    m_sprite->SetRect({left, 0.0f, side, side});
    m_track->SetRect({left, Size().y() - bar, side, bar});
    m_fill->SetRect({left, Size().y() - bar, side * std::clamp(m_soc, 0.0f, 1.0f), bar});
    m_track->SetCornerRadius(0.5f * bar);
    m_fill->SetCornerRadius(0.5f * bar);
}

auto BatteryComponent::Update() -> void
{
    if (m_device >= g_devices->All().size())
    {
        m_label.SetText("");
        ShowModel(nullptr, 0.0f, Eigen::Vector4f::Zero());
        return;
    }
    const auto& device = g_devices->All()[m_device];
    if (!device.soc)
    {
        m_label.SetText("");
        m_label.SetFgColor(Eigen::Vector4f::Zero());
        ShowModel(nullptr, 0.0f, Eigen::Vector4f::Zero());
        return;
    }
    const auto percent = static_cast<uint32_t>(std::min(*device.soc * 100.0f, 99.0f));
    Eigen::Vector4f color = m_normal_color;
    if (device.charging)
    {
        color = m_charging_color;
    }
    else if (*device.soc < m_low_threshold)
    {
        color = m_low_color;
    }
    const auto snapshot = m_sprite != nullptr && g_models != nullptr ? g_models->Snapshot(device.model) : nullptr;
    m_label.SetText(snapshot ? std::string() : std::format("{}{}", kDeviceTags[static_cast<size_t>(device.role)], percent));
    m_label.SetFgColor(color);
    ShowModel(snapshot, *device.soc, color);
}

auto BatteryComponent::ShowModel(const std::shared_ptr<Image>& snapshot, float soc, const Eigen::Vector4f& color) -> void
{
    if (m_sprite == nullptr)
    {
        return;
    }
    if (m_sprite->Sprite() != snapshot)
    {
        m_sprite->SetSprite(snapshot);
    }
    const Eigen::Vector4f track(color.x(), color.y(), color.z(), color.w() * kTrackAlpha);
    if (m_soc != soc || m_fill->BgColor() != color || m_track->BgColor() != track)
    {
        m_soc = soc;
        m_fill->SetBgColor(color);
        m_track->SetBgColor(track);
    }
}
