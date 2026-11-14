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

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ModelLoader.hpp"

class Image;

class DeviceModels final
{
  public:
    explicit DeviceModels(std::unique_ptr<ModelLoader> loader);

    auto Snapshot(const std::string& name) -> std::shared_ptr<Image>;

  private:
    std::unique_ptr<ModelLoader> m_loader;
    std::unordered_map<std::string, std::shared_ptr<Image>> m_snapshots;
    std::unordered_set<std::string> m_failed;
};

extern DeviceModels* g_models;
