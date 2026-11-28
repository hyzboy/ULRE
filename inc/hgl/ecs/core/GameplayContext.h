#pragma once

#include <hgl/ecs/core/ECSCoreContext.h>
#include <hgl/ecs/support/spatial/WorldSpatialState.h>

namespace hgl::ecs
{
    /**
     * ECSGameplayContext —— 无头 Gameplay / 物理 / 空间世界上下文。
     *
     * 组合：
     * - ECSCoreContext（实体、组件池、系统注册与 Tick 调度）
     * - WorldSpatialState（CPU 变换层级、包围盒、可见性集合）
     *
     * 边界约束：
     * - 零 Vulkan / 零 SceneGraph / 零 ULRE.ECS.Render 依赖
     * - 属于 ULRE.ECS.Core，适用于专用服务器、离线仿真与无头 Gameplay 测试
     */
    class ECSGameplayContext : public ECSCoreContext
    {
    private:
        WorldSpatialState spatial_state;

    protected:
        void ConfigureSystem(System *system) override;
        void OnEntityDestroying(EntityID id) override;

    public:
        explicit ECSGameplayContext(const char *name) : ECSGameplayContext(AnsiString(name)) {}
        explicit ECSGameplayContext(const AnsiString &name = "GameplayWorld");
        ~ECSGameplayContext() override;

        void Shutdown() override;

        WorldSpatialState &Spatial() { return spatial_state; }
        const WorldSpatialState &Spatial() const { return spatial_state; }
    };
}
