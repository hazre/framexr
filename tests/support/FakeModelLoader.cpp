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

#include "FakeModelLoader.hpp"

#include <array>

auto FakeModelLoader::Cube() -> ModelMesh
{
    ModelPart part;
    const std::array<Eigen::Vector3f, 6> normals{{
        Eigen::Vector3f::UnitX(),
        -Eigen::Vector3f::UnitX(),
        Eigen::Vector3f::UnitY(),
        -Eigen::Vector3f::UnitY(),
        Eigen::Vector3f::UnitZ(),
        -Eigen::Vector3f::UnitZ(),
    }};
    for (const auto& normal : normals)
    {
        const Eigen::Vector3f u = normal.cross(std::abs(normal.y()) > 0.5f ? Eigen::Vector3f::UnitX() : Eigen::Vector3f::UnitY());
        const Eigen::Vector3f v = normal.cross(u);
        const auto base = static_cast<uint32_t>(part.vertices.size());
        const std::array<Eigen::Vector2f, 4> corners{{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
        for (const auto& corner : corners)
        {
            part.vertices.push_back({
                .position = 0.05f * (normal + (corner.x() * u) + (corner.y() * v)),
                .normal = normal,
                .uv = 0.5f * (corner + Eigen::Vector2f::Ones()),
            });
        }
        part.indices.insert(part.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    part.texture_width = 2;
    part.texture_height = 2;
    part.texture = {230, 80, 60, 255, 60, 120, 230, 255, 60, 120, 230, 255, 230, 80, 60, 255};
    ModelMesh mesh;
    mesh.parts.push_back(std::move(part));
    return mesh;
}

auto FakeModelLoader::Load(const std::string& name) -> Result<std::optional<ModelMesh>>
{
    ++m_calls;
    if (name != "cube")
    {
        return Unexpected("unknown model");
    }
    if (m_calls == 1)
    {
        return std::optional<ModelMesh>();
    }
    return Cube();
}
