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
    }
}

namespace hgl::graph{

class TransformGizmoSystem : public hgl::ecs::System
{
private:
    GizmoECS *gizmo = nullptr;
    hgl::ecs::Entity *target_entity = nullptr;
    math::Vector3f initial_position = math::Vector3f(0.0f);
    GizmoMode default_mode;
    GizmoChangedCallback changed_callback;

    bool mode_switch_enabled = true;
    bool last_left_down = false;
    bool last_key_1 = false;
    bool last_key_2 = false;
    bool last_key_3 = false;
    bool last_key_4 = false;
    bool last_key_5 = false;

public:
    TransformGizmoSystem();
    ~TransformGizmoSystem() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    bool SetTargetEntity(hgl::ecs::Entity *entity);
    bool SetTargetTransform(const std::shared_ptr<hgl::ecs::TransformComponent> &transform);
    hgl::ecs::Entity *GetTargetEntity() const { return target_entity; }

    void SetModeSwitchEnabled(bool enabled) { mode_switch_enabled = enabled; }
    bool IsModeSwitchEnabled() const { return mode_switch_enabled; }

    void SetDefaultMode(GizmoMode mode);
    GizmoMode GetDefaultMode() const;
    GizmoMode GetCurrentMode() const;

    void SetInitialPosition(const math::Vector3f &position) { initial_position = position; }
    const math::Vector3f &GetInitialPosition() const { return initial_position; }

    void SetChangedCallback(GizmoChangedCallback callback);

    void SetAllowNegativeScale(bool enabled);
    bool IsAllowNegativeScale() const { return allow_negative_scale; }

    void SetFixedPixelDiameter(float pixel_diameter);
    float GetFixedPixelDiameter() const { return fixed_pixel_diameter; }

private:
    bool EnsureGizmo();
    bool resource_registered = false;
    bool allow_negative_scale = true;
    float fixed_pixel_diameter = 640.0f;
};

}//namespace hgl::graph
