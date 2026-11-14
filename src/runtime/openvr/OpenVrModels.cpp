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

#include "OpenVrModels.hpp"

#include <array>
#include <format>
#include <utility>
#include <vector>

#include <openvr.h>

#include "OpenVrMath.hpp"

namespace
{

struct VisiblePart
{
    std::string model;
    Eigen::Affine3f placement = Eigen::Affine3f::Identity();
};

auto ReadName(const auto& read) -> std::string
{
    std::array<char, vr::k_unMaxPropertyStringSize> buffer{};
    if (read(buffer.data(), static_cast<uint32_t>(buffer.size())) == 0)
    {
        return {};
    }
    return buffer.data();
}

auto VisibleParts(const std::string& name) -> std::vector<VisiblePart>
{
    auto* models = vr::VRRenderModels();
    std::vector<VisiblePart> parts;
    const uint32_t count = models->GetComponentCount(name.c_str());
    for (uint32_t index = 0; index < count; ++index)
    {
        const auto component = ReadName(
            [&](char* buffer, uint32_t size) -> uint32_t { return models->GetComponentName(name.c_str(), index, buffer, size); }
        );
        if (component.empty())
        {
            continue;
        }
        auto model = ReadName(
            [&](char* buffer, uint32_t size) -> uint32_t
            { return models->GetComponentRenderModelName(name.c_str(), component.c_str(), buffer, size); }
        );
        vr::RenderModel_ControllerMode_State_t mode{};
        vr::RenderModel_ComponentState_t state{};
        if (model.empty()
            || !models
                    ->GetComponentStateForDevicePath(name.c_str(), component.c_str(), vr::k_ulInvalidInputValueHandle, &mode, &state)
            || (state.uProperties & vr::VRComponentProperty_IsVisible) == 0)
        {
            continue;
        }
        parts.push_back({.model = std::move(model), .placement = OpenVrMath::ToAffine(state.mTrackingToComponentRenderModel)});
    }
    if (parts.empty())
    {
        parts.push_back({.model = name});
    }
    return parts;
}

} // namespace

auto OpenVrModels::Load(const std::string& name) -> Result<std::optional<ModelMesh>>
{
    if (vr::VRRenderModels() == nullptr)
    {
        return Unexpected("OpenVR render models are unavailable");
    }
    ModelMesh mesh;
    for (const auto& part : VisibleParts(name))
    {
        ASSIGN_OR_RETURN(auto loaded, LoadPart(part.model, part.placement));
        if (!loaded)
        {
            return std::optional<ModelMesh>();
        }
        mesh.parts.push_back(std::move(*loaded));
    }
    return mesh;
}

auto OpenVrModels::LoadPart(const std::string& name, const Eigen::Affine3f& placement) -> Result<std::optional<ModelPart>>
{
    auto* models = vr::VRRenderModels();
    vr::RenderModel_t* model = nullptr;
    const auto error = models->LoadRenderModel_Async(name.c_str(), &model);
    if (error == vr::VRRenderModelError_Loading)
    {
        return std::optional<ModelPart>();
    }
    if (error != vr::VRRenderModelError_None || model == nullptr)
    {
        return Unexpected(std::format("{} ({})", models->GetRenderModelErrorNameFromEnum(error), name));
    }

    vr::RenderModel_TextureMap_t* texture = nullptr;
    if (model->diffuseTextureId >= 0
        && models->LoadTexture_Async(model->diffuseTextureId, &texture) == vr::VRRenderModelError_Loading)
    {
        models->FreeRenderModel(model);
        return std::optional<ModelPart>();
    }

    ModelPart part;
    const Eigen::Matrix3f rotation = placement.linear();
    part.vertices.reserve(model->unVertexCount);
    for (uint32_t index = 0; index < model->unVertexCount; ++index)
    {
        const auto& vertex = model->rVertexData[index];
        const Eigen::Vector3f position(vertex.vPosition.v[0], vertex.vPosition.v[1], vertex.vPosition.v[2]);
        const Eigen::Vector3f normal(vertex.vNormal.v[0], vertex.vNormal.v[1], vertex.vNormal.v[2]);
        part.vertices.push_back({
            .position = placement * position,
            .normal = rotation * normal,
            .uv = Eigen::Vector2f(vertex.rfTextureCoord[0], vertex.rfTextureCoord[1]),
        });
    }
    part.indices.assign(model->rIndexData, model->rIndexData + (static_cast<size_t>(model->unTriangleCount) * 3));
    if (texture != nullptr && texture->format == vr::VRRenderModelTextureFormat_RGBA8_SRGB)
    {
        part.texture_width = texture->unWidth;
        part.texture_height = texture->unHeight;
        part.texture.assign(
            texture->rubTextureMapData,
            texture->rubTextureMapData + (static_cast<size_t>(texture->unWidth) * texture->unHeight * 4)
        );
    }
    models->FreeTexture(texture);
    models->FreeRenderModel(model);
    return part;
}
