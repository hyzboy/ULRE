#pragma once
#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>

namespace hgl::graph::data
{
    using namespace hgl::math;

    /**
     * 通用顶点结构
     * Universal vertex structure for geometry data
     */
    struct Vertex
    {
        Vector3f position;
        Vector3f normal = Vector3f(0, 0, 0);
        Vector3f tangent = Vector3f(0, 0, 0);
        Vector2f texcoord = Vector2f(0, 0);
        Vector4f color = Vector4f(1, 1, 1, 1);

        Vertex() = default;
        Vertex(const Vector3f &pos) : position(pos) {}
        Vertex(float x, float y, float z) : position(x, y, z) {}
    };
}
