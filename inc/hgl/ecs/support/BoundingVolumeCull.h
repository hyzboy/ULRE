#pragma once

/**
 * 视锥剔除时使用的包围体尺寸换算辅助函数。
 *
 * 背景：PrimitiveComponent 的包围半径、BoundingBoxComponent 的本地包围盒，
 * 都是在“几何体本地空间”中量算出来的，本身**不含**实体 Transform 的缩放。
 * 直接拿去测视锥会对被放大过的实体严重低估——例如 500 倍缩放的地面，
 * 半径仍按 0.707 参与判定，中心一旦落到视锥外就会被整块剔除。
 * 因此本地尺寸必须乘上世界缩放后才能用于视锥判定。
 */

#include<glm/glm.hpp>

namespace hgl::ecs
{
    /** 世界矩阵中的最大轴向缩放（三轴列向量长度取最大者）；退化矩阵返回 1.0 */
    inline float GetMaxWorldScale(const glm::mat4 &world_matrix)
    {
        const float sx = glm::length(glm::vec3(world_matrix[0]));
        const float sy = glm::length(glm::vec3(world_matrix[1]));
        const float sz = glm::length(glm::vec3(world_matrix[2]));
        const float max_scale = glm::max(sx, glm::max(sy, sz));

        return max_scale > 0.0f ? max_scale : 1.0f;
    }

    /** 把本地空间求得的包围半径换算为世界空间半径 */
    inline float ToWorldBoundingRadius(float local_radius, const glm::mat4 &world_matrix)
    {
        return local_radius * GetMaxWorldScale(world_matrix);
    }
}//namespace hgl::ecs
