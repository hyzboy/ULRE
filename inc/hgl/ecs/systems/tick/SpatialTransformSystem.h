#pragma once

#include <hgl/ecs/core/system/System.h>
#include <hgl/ecs/support/spatial/WorldSpatialState.h>

namespace hgl::ecs
{
    /**
     * SpatialTransformSystem
     *
     * 纯 CPU 变换更新系统（位于 ULRE.ECS.Core，零 Vulkan / 渲染依赖）。
     * 在 ExecutionPhase::TickTransform 阶段驱动 WorldSpatialState::UpdateTransforms()，
     * 供无头 Gameplay 世界直接计算父子层级与世界矩阵。
     */
    class SpatialTransformSystem : public System
    {
    private:
        WorldSpatialState *spatial_state = nullptr;

    public:
        explicit SpatialTransformSystem(WorldSpatialState *spatial = nullptr,
                                        const AnsiString &name = "SpatialTransformSystem");
        ~SpatialTransformSystem() override = default;

        void SetSpatialState(WorldSpatialState *s) { spatial_state = s; }
        WorldSpatialState *GetSpatialState() const { return spatial_state; }

        void Update(float deltaTime) override;
    };
}
