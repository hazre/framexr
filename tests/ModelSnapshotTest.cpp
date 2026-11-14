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

#include <gtest/gtest.h>

#include <numbers>

#include "FakeModelLoader.hpp"
#include "ModelSnapshot.hpp"

namespace
{

constexpr uint32_t kSize = 32;

auto Alpha(const std::vector<std::byte>& pixels, uint32_t x, uint32_t y) -> int
{
    return std::to_integer<int>(pixels[((static_cast<size_t>(y) * kSize) + x) * 4 + 3]);
}

auto View() -> Eigen::Quaternionf
{
    return Eigen::Quaternionf(Eigen::AngleAxisf(0.4f, Eigen::Vector3f::UnitX()))
         * Eigen::Quaternionf(Eigen::AngleAxisf(0.6f, Eigen::Vector3f::UnitY()));
}

} // namespace

TEST(ModelSnapshot, FillsTheMiddleAndLeavesTheCornersClear)
{
    const auto pixels = ModelSnapshot::Render(FakeModelLoader::Cube(), kSize, View());
    ASSERT_EQ(pixels.size(), static_cast<size_t>(kSize) * kSize * 4);
    EXPECT_EQ(Alpha(pixels, kSize / 2, kSize / 2), 255);
    EXPECT_EQ(Alpha(pixels, 0, 0), 0);
    EXPECT_EQ(Alpha(pixels, kSize - 1, kSize - 1), 0);
}

TEST(ModelSnapshot, SamplesTheTexture)
{
    const auto pixels = ModelSnapshot::Render(FakeModelLoader::Cube(), kSize, View());
    bool red = false;
    bool blue = false;
    for (size_t offset = 0; offset < pixels.size(); offset += 4)
    {
        const int r = std::to_integer<int>(pixels[offset]);
        const int b = std::to_integer<int>(pixels[offset + 2]);
        red = red || r > b + 60;
        blue = blue || b > r + 60;
    }
    EXPECT_TRUE(red);
    EXPECT_TRUE(blue);
}

TEST(ModelSnapshot, EmptyMeshIsTransparent)
{
    const auto pixels = ModelSnapshot::Render(ModelMesh{}, kSize, View());
    ASSERT_EQ(pixels.size(), static_cast<size_t>(kSize) * kSize * 4);
    for (size_t offset = 3; offset < pixels.size(); offset += 4)
    {
        ASSERT_EQ(std::to_integer<int>(pixels[offset]), 0);
    }
}
