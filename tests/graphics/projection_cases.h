// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "HLEGraphics/VertexProjectionCache.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>

static bool close_position(const glm::vec4 &a, const glm::vec4 &b)
{
    const glm::vec3 x = glm::vec3(a) / a.w, y = glm::vec3(b) / b.w;
    return glm::all(glm::lessThanEqual(glm::abs(x - y), glm::vec3(0.0002f)));
}
static int projection_fail(const char *message)
{
    std::fprintf(stderr, "Projection regression: %s\n", message);
    return 1;
}
static glm::mat4 camera()
{
    return glm::perspective(glm::radians(60.f), 4.f / 3.f, 1.f, 100.f);
}
static glm::mat4 bone(float angle, float x)
{
    return camera() * glm::translate(glm::mat4(1.f), glm::vec3(x, .3f, -.5f)) *
        glm::rotate(glm::mat4(1.f), angle, glm::vec3(0.f, 1.f, 0.f));
}
static int mixed_projection_triangle()
{
    VertexProjectionCache<80> cache;
    const glm::mat4 first = bone(.3f, .8f), second = bone(-.2f, -1.f);
    glm::vec4 positions[] = {{-1,-1,-8,1},{1,-1,-8,1},{0,1,-8,1}};
    const glm::vec4 clip[] = {first * positions[0], first * positions[1], second * positions[2]};
    cache.SetProjection(first);
    cache.Loaded(0, 2);
    cache.SetProjection(second);
    cache.Loaded(2, 1);
    // This is the original bug: reusing XYZ with the later projection moves
    // the earlier bone's vertices, even though N64 keeps their clip positions.
    if (close_position(second * positions[0], clip[0])) return projection_fail("Control did not reproduce the old matrix mismatch.");
    for (unsigned i=0; i<3; ++i) {
        const bool changed = cache.Rebase(i, clip[i], positions[i]);
        if (changed != (i<2)) return projection_fail("Current-projection vertex was unnecessarily changed.");
        if (!close_position(second * positions[i], clip[i])) return projection_fail("A mixed-bone triangle changed screen position or depth.");
        if (!glm::all(glm::lessThanEqual(glm::abs(second*positions[i]-clip[i]),glm::vec4(.0002f))))
            return projection_fail("Affine bone change lost clip W/perspective interpolation.");
    }
    return 0;
}
static int unchanged_projection()
{
    VertexProjectionCache<80> cache;
    const auto projection = camera();
    const glm::vec4 original(1.125f,-.25f,-4.f,1.f);
    glm::vec4 position=original;
    cache.SetProjection(projection); cache.Loaded(79,1);
    cache.SetProjection(projection);
    if (cache.Rebase(79,projection*position,position) || position!=original)
        return projection_fail("The common single-projection path changed XYZ.");
    return 0;
}
static int repeated_projection_changes()
{
    VertexProjectionCache<80> cache;
    glm::vec4 position(-.4f,.8f,-9.f,1.f);
    const auto initial=bone(.1f,.2f);
    const glm::vec4 clip=initial*position;
    cache.SetProjection(initial);cache.Loaded(17,1);
    for (unsigned i=0;i<1000;++i) {
        const auto projection=bone(-.8f+i*.0015f,-.5f+i*.001f);
        cache.SetProjection(projection);
        cache.Rebase(17,clip,position);
        if (!close_position(projection*position,clip)) return projection_fail("Changing bones accumulated projection error.");
    }
    return 0;
}
static int copied_and_reloaded_vertices()
{
    VertexProjectionCache<80> cache;
    const auto first=bone(.4f,1.f), second=bone(-.4f,-1.f);
    glm::vec4 position(.5f,-.2f,-6.f,1.f);
    const auto clip=first*position;
    cache.SetProjection(first);cache.Loaded(3,1);
    cache.SetProjection(second);cache.Copy(3,9);
    if (!cache.Rebase(9,clip,position) || !close_position(second*position,clip))
        return projection_fail("Copied vertex lost its projection provenance.");
    cache.Loaded(9,1);
    const auto before=position;
    if (cache.Rebase(9,second*position,position) || position!=before)
        return projection_fail("Reloaded vertex retained an older projection.");
    return 0;
}
static int homogeneous_and_reset()
{
    VertexProjectionCache<80> cache;
    glm::vec4 position(.4f,.5f,-12.f,1.f);
    const auto first=glm::perspective(glm::radians(45.f),1.6f,.5f,50.f);
    const auto clip=first*position;
    cache.SetProjection(first);cache.Loaded(0,1);
    const auto second=glm::perspective(glm::radians(70.f),1.2f,1.f,100.f);
    cache.SetProjection(second);cache.Rebase(0,clip,position);
    if (position.w!=1.f || !close_position(second*position,clip)) return projection_fail("XYZ-only GU input was not normalized.");
    cache.Reset();
    if (!cache.Rebase(0,clip,position) || !close_position(position,clip)) return projection_fail("New frame retained an old projection.");
    return 0;
}
static int projection_cases()
{
    return mixed_projection_triangle()+unchanged_projection()+repeated_projection_changes()+
        copied_and_reloaded_vertices()+homogeneous_and_reset();
}
