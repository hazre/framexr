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

#include "ModelSnapshot.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

constexpr uint32_t kSamples = 3;
constexpr float kMargin = 0.04f;
constexpr float kAmbient = 0.4f;
constexpr float kDiffuse = 0.7f;
constexpr float kSpecular = 0.2f;
constexpr float kShininess = 24.0f;
constexpr float kInsideEpsilon = -1e-5f;

auto Edge(const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector2f& point) -> float
{
    return ((point.x() - a.x()) * (b.y() - a.y())) - ((point.y() - a.y()) * (b.x() - a.x()));
}

auto Untextured() -> Eigen::Vector3f
{
    return {0.55f, 0.57f, 0.62f};
}

auto Texel(const ModelPart& part, uint32_t x, uint32_t y) -> Eigen::Vector3f
{
    const size_t offset = ((static_cast<size_t>(y) * part.texture_width) + x) * 4;
    return Eigen::Vector3f(part.texture[offset], part.texture[offset + 1], part.texture[offset + 2]) / 255.0f;
}

auto Sample(const ModelPart& part, const Eigen::Vector2f& uv) -> Eigen::Vector3f
{
    const size_t needed = static_cast<size_t>(part.texture_width) * part.texture_height * 4;
    if (part.texture_width == 0 || part.texture_height == 0 || part.texture.size() < needed)
    {
        return Untextured();
    }
    const float u = (uv.x() - std::floor(uv.x())) * static_cast<float>(part.texture_width) - 0.5f;
    const float v = (uv.y() - std::floor(uv.y())) * static_cast<float>(part.texture_height) - 0.5f;
    const float fx = u - std::floor(u);
    const float fy = v - std::floor(v);
    const auto wrap = [](float value, uint32_t extent) -> uint32_t
    {
        const auto count = static_cast<int64_t>(extent);
        return static_cast<uint32_t>(((static_cast<int64_t>(std::floor(value)) % count) + count) % count);
    };
    const uint32_t x0 = wrap(u, part.texture_width);
    const uint32_t y0 = wrap(v, part.texture_height);
    const uint32_t x1 = (x0 + 1) % part.texture_width;
    const uint32_t y1 = (y0 + 1) % part.texture_height;
    const Eigen::Vector3f top = ((1.0f - fx) * Texel(part, x0, y0)) + (fx * Texel(part, x1, y0));
    const Eigen::Vector3f bottom = ((1.0f - fx) * Texel(part, x0, y1)) + (fx * Texel(part, x1, y1));
    return ((1.0f - fy) * top) + (fy * bottom);
}

auto Shade(const Eigen::Vector3f& albedo, Eigen::Vector3f normal) -> Eigen::Vector3f
{
    const Eigen::Vector3f light = Eigen::Vector3f(-0.35f, 0.75f, 0.55f).normalized();
    const Eigen::Vector3f half = (light + Eigen::Vector3f::UnitZ()).normalized();
    if (normal.z() < 0.0f)
    {
        normal = -normal;
    }
    const float diffuse = std::max(normal.dot(light), 0.0f);
    const float specular = std::pow(std::max(normal.dot(half), 0.0f), kShininess);
    return ((albedo * (kAmbient + (kDiffuse * diffuse))) + Eigen::Vector3f::Constant(kSpecular * specular)).cwiseMin(1.0f);
}

} // namespace

auto ModelSnapshot::Render(const ModelMesh& mesh, uint32_t size, const Eigen::Quaternionf& view) -> std::vector<std::byte>
{
    std::vector<std::byte> pixels(static_cast<size_t>(size) * size * 4, std::byte{0});
    const Eigen::Matrix3f rotation = view.normalized().toRotationMatrix();
    Eigen::AlignedBox3f bounds;
    for (const auto& part : mesh.parts)
    {
        for (const auto& vertex : part.vertices)
        {
            bounds.extend(rotation * vertex.position);
        }
    }
    if (size == 0 || bounds.isEmpty())
    {
        return pixels;
    }

    const uint32_t span = size * kSamples;
    const auto extent = static_cast<float>(span);
    const Eigen::Vector3f sizes = bounds.sizes();
    const float fit = (1.0f - (2.0f * kMargin)) * extent / std::max({sizes.x(), sizes.y(), 1e-6f});
    const Eigen::Vector3f center = bounds.center();
    std::vector<Eigen::Vector3f> color(static_cast<size_t>(span) * span, Eigen::Vector3f::Zero());
    std::vector<float> depth(static_cast<size_t>(span) * span, std::numeric_limits<float>::lowest());
    std::vector<bool> covered(static_cast<size_t>(span) * span, false);

    for (const auto& part : mesh.parts)
    {
        std::vector<Eigen::Vector3f> screen;
        std::vector<Eigen::Vector3f> normals;
        screen.reserve(part.vertices.size());
        normals.reserve(part.vertices.size());
        for (const auto& vertex : part.vertices)
        {
            const Eigen::Vector3f placed = (rotation * vertex.position) - center;
            screen.emplace_back((0.5f * extent) + (placed.x() * fit), (0.5f * extent) - (placed.y() * fit), placed.z());
            normals.push_back(rotation * vertex.normal);
        }
        for (size_t i = 0; i + 2 < part.indices.size(); i += 3)
        {
            const uint32_t a = part.indices[i];
            const uint32_t b = part.indices[i + 1];
            const uint32_t c = part.indices[i + 2];
            if (a >= screen.size() || b >= screen.size() || c >= screen.size())
            {
                continue;
            }
            const auto& p0 = screen[a];
            const auto& p1 = screen[b];
            const auto& p2 = screen[c];
            const float area = Edge(p0, p1, p2.head<2>());
            if (std::abs(area) < 1e-9f)
            {
                continue;
            }
            const float min_x = std::max(std::floor(std::min({p0.x(), p1.x(), p2.x()})), 0.0f);
            const float max_x = std::min(std::ceil(std::max({p0.x(), p1.x(), p2.x()})), extent - 1.0f);
            const float min_y = std::max(std::floor(std::min({p0.y(), p1.y(), p2.y()})), 0.0f);
            const float max_y = std::min(std::ceil(std::max({p0.y(), p1.y(), p2.y()})), extent - 1.0f);
            for (auto y = static_cast<uint32_t>(min_y); static_cast<float>(y) <= max_y; ++y)
            {
                for (auto x = static_cast<uint32_t>(min_x); static_cast<float>(x) <= max_x; ++x)
                {
                    const Eigen::Vector2f point(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
                    const float w0 = Edge(p1, p2, point) / area;
                    const float w1 = Edge(p2, p0, point) / area;
                    const float w2 = 1.0f - w0 - w1;
                    if (w0 < kInsideEpsilon || w1 < kInsideEpsilon || w2 < kInsideEpsilon)
                    {
                        continue;
                    }
                    const size_t index = (static_cast<size_t>(y) * span) + x;
                    const float z = (w0 * p0.z()) + (w1 * p1.z()) + (w2 * p2.z());
                    if (z <= depth[index])
                    {
                        continue;
                    }
                    const Eigen::Vector2f uv = (w0 * part.vertices[a].uv) + (w1 * part.vertices[b].uv) + (w2 * part.vertices[c].uv);
                    const Eigen::Vector3f normal = (w0 * normals[a]) + (w1 * normals[b]) + (w2 * normals[c]);
                    depth[index] = z;
                    covered[index] = true;
                    color[index] = Shade(Sample(part, uv), normal.normalized());
                }
            }
        }
    }

    for (uint32_t y = 0; y < size; ++y)
    {
        for (uint32_t x = 0; x < size; ++x)
        {
            Eigen::Vector3f sum = Eigen::Vector3f::Zero();
            uint32_t count = 0;
            for (uint32_t sy = 0; sy < kSamples; ++sy)
            {
                for (uint32_t sx = 0; sx < kSamples; ++sx)
                {
                    const size_t index = (static_cast<size_t>((y * kSamples) + sy) * span) + (x * kSamples) + sx;
                    if (covered[index])
                    {
                        sum += color[index];
                        ++count;
                    }
                }
            }
            if (count == 0)
            {
                continue;
            }
            const Eigen::Vector3f average = sum / static_cast<float>(count);
            const size_t offset = ((static_cast<size_t>(y) * size) + x) * 4;
            for (size_t channel = 0; channel < 3; ++channel)
            {
                pixels[offset + channel] = static_cast<std::byte>(
                    std::lround(std::clamp(average[static_cast<Eigen::Index>(channel)], 0.0f, 1.0f) * 255.0f)
                );
            }
            pixels[offset + 3] = static_cast<std::byte>((count * 255) / (kSamples * kSamples));
        }
    }
    return pixels;
}
