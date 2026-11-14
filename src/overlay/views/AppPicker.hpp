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

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "Capture.hpp"
#include "Error.hpp"
#include "View.hpp"

constexpr std::string_view kAppPickerName = "app-picker";

class AppPicker final : public View
{
  public:
    static auto Open(std::optional<std::string> replace) -> void;

  private:
    AppPicker() = default;

    static auto Create(const std::optional<std::string>& replace) -> std::unique_ptr<View>;
    static auto ShowApp(std::string name, std::future<Result<std::unique_ptr<Capture>>> capture) -> void;
    static auto UniqueName(const std::string& title) -> std::string;
};
