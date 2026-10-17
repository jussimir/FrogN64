// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

// N64 vertices keep their clip position when a later matrix command changes
// the projection. PSP GU accepts only XYZ, so cached vertices from an older
// projection must be expressed in the current projection's coordinate space.
template <std::size_t Capacity>
class VertexProjectionCache
{
public:
    void Reset()
    {
        mProjection = glm::mat4(1.f);
        mInverse = mProjection;
        mVersion = mInverseVersion = 1;
        mVertexVersions.fill(0);
    }

    void SetProjection(const glm::mat4 &projection)
    {
        if (projection == mProjection) return;
        mProjection = projection;
        if (++mVersion == 0) {
            mVersion = 1;
            mVertexVersions.fill(0);
            mInverseVersion = 0;
        }
    }

    void Loaded(std::size_t first, std::size_t count)
    {
        std::fill_n(mVertexVersions.begin() + first, count, mVersion);
    }

    void Copy(std::size_t source, std::size_t destination)
    {
        mVertexVersions[destination] = mVertexVersions[source];
    }

    bool Rebase(std::size_t index, const glm::vec4 &clip, glm::vec4 &position)
    {
        if (mVertexVersions[index] == mVersion) return false;
        if (mInverseVersion != mVersion) {
            mInverse = glm::inverse(mProjection);
            mInverseVersion = mVersion;
        }
        const glm::vec4 homogeneous = mInverse * clip;
        position = homogeneous / homogeneous.w;
        mVertexVersions[index] = mVersion;
        return true;
    }

private:
    alignas(16) glm::mat4 mProjection{1.f};
    alignas(16) glm::mat4 mInverse{1.f};
    std::array<std::uint32_t, Capacity> mVertexVersions{};
    std::uint32_t mVersion = 1;
    std::uint32_t mInverseVersion = 1;
};
