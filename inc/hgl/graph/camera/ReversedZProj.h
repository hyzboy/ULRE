#pragma once

#include <hgl/math/Matrix.h>

namespace hgl::graph
{
    using namespace hgl::math;

    /**
     * Reversed-Z + Infinite Far Plane 透视投影矩阵
     *
     * 深度范围：near → 1.0, ∞ → 0.0
     * 必须配合 VK_COMPARE_OP_GREATER 和 depth clear = 0.0f 使用
     *
     * @param fov_y_radians 垂直视场角 (radians)
     * @param aspect 宽高比
     * @param near_z 近平面距离（必须 > 0）
     */
    Matrix4f MakeInfiniteReversedZProj(float fov_y_radians, float aspect, float near_z);
}
