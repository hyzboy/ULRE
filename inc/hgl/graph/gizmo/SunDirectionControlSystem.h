#pragma once

#include"GizmoTypes.h"
#include<hgl/ecs/core/System.h>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        class Entity;
        class TransformComponent;
        class EnvironmentSystem;
    }
}

namespace hgl::graph{

class SunDirectionControlSystem : public hgl::ecs::System
{
private:
    GizmoECS *gizmo = nullptr;
    hgl::ecs::Entity *proxy_entity = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> proxy_transform;
    hgl::ecs::EnvironmentSystem *environment_system = nullptr;

    math::Vector3f gizmo_position = math::Vector3f(0.0f);
    bool auto_find_environment = true;

    bool last_left_down = false;
    bool resource_registered = false;

public:
    SunDirectionControlSystem();
    ~SunDirectionControlSystem() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    void SetEnvironmentSystem(hgl::ecs::EnvironmentSystem *env_system)
    {
        environment_system = env_system;
        auto_find_environment = (env_system == nullptr);
    }

    hgl::ecs::EnvironmentSystem *GetEnvironmentSystem() const { return environment_system; }

    void SetGizmoPosition(const math::Vector3f &position) { gizmo_position = position; }
    const math::Vector3f &GetGizmoPosition() const { return gizmo_position; }

    void SetGizmoVisible(bool visible);

private:
    bool EnsureEnvironment();
    bool EnsureProxyEntity();
    bool EnsureGizmo();
};

}//namespace hgl::graph
