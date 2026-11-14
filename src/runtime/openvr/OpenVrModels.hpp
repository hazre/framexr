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

#include <optional>
#include <string>

#include <Eigen/Geometry>

#include "ModelLoader.hpp"

class OpenVrModels final : public ModelLoader
{
  public:
    OpenVrModels() = default;

    auto Load(const std::string& name) -> Result<std::optional<ModelMesh>> override;

  private:
    static auto LoadPart(const std::string& name, const Eigen::Affine3f& placement) -> Result<std::optional<ModelPart>>;
};
