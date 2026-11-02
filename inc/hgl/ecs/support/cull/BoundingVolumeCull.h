#pragma once

/**
 * 视锥剔除时使用的包围体尺寸换算辅助函数。
 *
 * 背景：GeometryData 的包围半径、BoundingBoxDataStorage 的本地包围盒，
 * 都是在“几何体本地空间”中量算出来的，本身**不含**实体 Transform 的缩放。
 * 直接拿去测视锥会对被放大过的实体严重低估——例如 500 倍缩放的地面，
 * 半径仍按 0.707 参与判定，中心一旦落到视锥外就会被整块剔除。
 * 因此本地尺寸必须乘上世界缩放后才能用于视锥判定。
 */

#include<glm/glm.hpp>

namespace hgl::ecs
{
    /**
     * 世界矩阵中的最大轴向缩放（三轴列向量长度取最大者）——**公式的唯一定义点**。
     *
     * **不做退化保护**：退化（scale == 0）时返回 0.0。这是与 GPU 剔除逐字同式的形态，
     * GLSL 镜像在 `ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl`（`float scale = max(length(...), ...)`），
     * 改这里必须同步改那里（两边都不带保护时才能互为参考对拍）。
     */
    inline float GetMaxWorldScaleRaw(const glm::mat4 &world_matrix)
    {
        const float sx = glm::length(glm::vec3(world_matrix[0]));
        const float sy = glm::length(glm::vec3(world_matrix[1]));
        const float sz = glm::length(glm::vec3(world_matrix[2]));

        return glm::max(sx, glm::max(sy, sz));
    }

    /**
     * 最大轴向缩放 + **退化保护**（退化矩阵返回 1.0）：CPU 侧包围体判定的既有语义
     * （`PrimitiveBatchPipeline` 的局部 AABB / 包围半径两条路；`TestBoundingVolumeCull` 钉住它）。
     *
     * 注意：CPU 剔除**参考**（`CPUCullReferenceSystem`，用于与 GPU 对拍）该用 `Raw`——
     * 用这一条会在 scale == 0 时让 CPU 判「可见」而 GPU 判「不可见」，正是在这对系统要检的
     * 维度上制造假差异。
     */
    inline float GetMaxWorldScale(const glm::mat4 &world_matrix)
    {
        const float max_scale = GetMaxWorldScaleRaw(world_matrix);

        return max_scale > 0.0f ? max_scale : 1.0f;
    }

    /** 把本地空间求得的包围半径换算为世界空间半径 */
    inline float ToWorldBoundingRadius(float local_radius, const glm::mat4 &world_matrix)
    {
        return local_radius * GetMaxWorldScale(world_matrix);
    }
}//namespace hgl::ecs
