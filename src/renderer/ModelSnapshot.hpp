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

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

struct ModelVertex
{
    Eigen::Vector3f position = Eigen::Vector3f::Zero();
    Eigen::Vector3f normal = Eigen::Vector3f::UnitZ();
    Eigen::Vector2f uv = Eigen::Vector2f::Zero();
};

struct ModelPart
{
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t texture_width = 0;
    uint32_t texture_height = 0;
    std::vector<uint8_t> texture;
};

struct ModelMesh
{
    std::vector<ModelPart> parts;
};

class ModelSnapshot final
{
  public:
    static auto Render(const ModelMesh& mesh, uint32_t size, const Eigen::Quaternionf& view) -> std::vector<std::byte>;
};
