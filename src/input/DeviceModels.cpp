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

#include "DeviceModels.hpp"

#include <numbers>
#include <utility>

#include "CommandBuffer.hpp"
#include "Device.hpp"
#include "Image.hpp"
#include "Logger.hpp"

DeviceModels* g_models = nullptr;

namespace
{

constexpr uint32_t kSnapshotSize = 128;
constexpr float kYaw = 330.0f;
constexpr float kPitch = -110.0f;
constexpr float kRoll = 180.0f;

auto View() -> Eigen::Quaternionf
{
    constexpr float kRadians = std::numbers::pi_v<float> / 180.0f;
    return Eigen::Quaternionf(Eigen::AngleAxisf(kRoll * kRadians, Eigen::Vector3f::UnitZ()))
         * Eigen::Quaternionf(Eigen::AngleAxisf(kPitch * kRadians, Eigen::Vector3f::UnitX()))
         * Eigen::Quaternionf(Eigen::AngleAxisf(kYaw * kRadians, Eigen::Vector3f::UnitY()));
}

auto Upload(const std::vector<std::byte>& pixels) -> Result<std::shared_ptr<Image>>
{
    if (g_device == nullptr)
    {
        return Unexpected("No graphics device");
    }
    ASSIGN_OR_RETURN(auto command_buffer, g_device->CreateCommandBuffer(g_device->TransferQueue()));
    ASSIGN_OR_RETURN(auto image, command_buffer.UploadImage(kSnapshotSize, kSnapshotSize, VK_FORMAT_R8G8B8A8_UNORM, pixels));
    RETURN_IF_ERROR(command_buffer.SubmitUpload());
    return image;
}

} // namespace

DeviceModels::DeviceModels(std::unique_ptr<ModelLoader> loader) : m_loader(std::move(loader))
{
}

auto DeviceModels::Snapshot(const std::string& name) -> std::shared_ptr<Image>
{
    if (name.empty() || m_loader == nullptr || m_failed.contains(name))
    {
        return nullptr;
    }
    if (const auto it = m_snapshots.find(name); it != m_snapshots.end())
    {
        return it->second;
    }
    auto loaded = m_loader->Load(name);
    if (loaded && !*loaded)
    {
        return nullptr;
    }
    auto image = loaded.and_then(
        [](const std::optional<ModelMesh>& mesh) -> Result<std::shared_ptr<Image>>
        { return Upload(ModelSnapshot::Render(*mesh, kSnapshotSize, View())); }
    );
    if (!image)
    {
        LOG_WARN("Could not draw render model {}: {}", name, image.error().Message());
        m_failed.insert(name);
        return nullptr;
    }
    m_snapshots.emplace(name, *image);
    return *image;
}
