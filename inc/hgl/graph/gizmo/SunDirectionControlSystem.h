#pragma once

#include"GizmoTypes.h"
#include<hgl/ecs/core/System.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        class Entity;
        class LightSystem;
    }
}

namespace hgl::graph{

class SunDirectionControlSystem : public hgl::ecs::System
{
private:
    GizmoECS *gizmo = nullptr;
    hgl::ecs::Entity *proxy_entity = nullptr;
    hgl::ecs::TransformAccessor proxy_transform;
    /// 光源域（T6b）：太阳方向住在**世界私有光源行 0**，本 gizmo 是它的编辑入口之一。
    hgl::ecs::LightSystem *light_system = nullptr;

    math::Vector3f gizmo_position = math::Vector3f(0.0f);
    bool auto_find_light = true;

    bool last_left_down = false;
    bool resource_registered = false;

public:
    SunDirectionControlSystem();
    ~SunDirectionControlSystem() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    void SetLightSystem(hgl::ecs::LightSystem *sys)
    {
        light_system = sys;
        auto_find_light = (sys == nullptr);
    }

    hgl::ecs::LightSystem *GetLightSystem() const { return light_system; }

    void SetGizmoPosition(const math::Vector3f &position) { gizmo_position = position; }
    const math::Vector3f &GetGizmoPosition() const { return gizmo_position; }

    void SetGizmoVisible(bool visible);

private:
    bool EnsureLightSystem();
    bool EnsureProxyEntity();
    bool EnsureGizmo();
};

}//namespace hgl::graph
