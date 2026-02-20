#pragma once

#include<hgl/CoreType.h>
#include<hgl/vk/VK.h>
#include<hgl/math/VectorTypes.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/ecs/core/System.h>
#include<glm/gtc/quaternion.hpp>
#include<functional>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
        class World;
        class Entity;
        class InputSystem;
        class CameraSystem;
        class TransformComponent;
        class EnvironmentSystem;
    }
}

namespace hgl::graph{

// 统一 Gizmo 世界（推荐使用）
class RenderPass;
struct GizmoECS;

enum class GizmoMode : int
{
    MoveWorld = 1,
    MoveLocal = 2,
    RotateWorld = 3,
    RotateLocal = 4,
    ScaleLocal = 5,

    Move = MoveWorld,
    Rotate = RotateWorld,
    Scale = ScaleLocal
};

struct GizmoTransformChange
{
    math::Vector3f previous_position;
    math::Vector3f current_position;
    glm::quat previous_rotation;
    glm::quat current_rotation;
    math::Vector3f previous_scale;
    math::Vector3f current_scale;
    GizmoMode mode = GizmoMode::MoveWorld;
};

using GizmoChangedCallback = std::function<void(const GizmoTransformChange&)>;

/// ============= Unified Gizmo Interface (Recommended for external users) =============
/// 外部开发者建议只使用以下 Unified/System API。
/// Move/Rotate/Scale 与资源层细节已转入内部头（GizmoInternal.h）。

class TransformGizmoSystem : public hgl::ecs::System
{
private:
    GizmoECS *gizmo = nullptr;
    hgl::ecs::Entity *target_entity = nullptr;
    math::Vector3f initial_position = math::Vector3f(0.0f);
    GizmoMode default_mode = GizmoMode::MoveWorld;
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

    void SetDefaultMode(GizmoMode mode) { default_mode = mode; }
    GizmoMode GetDefaultMode() const { return default_mode; }
    GizmoMode GetCurrentMode() const;

    void SetInitialPosition(const math::Vector3f &position) { initial_position = position; }
    const math::Vector3f &GetInitialPosition() const { return initial_position; }

    void SetChangedCallback(GizmoChangedCallback callback);

    void SetAllowNegativeScale(bool enabled);
    bool IsAllowNegativeScale() const { return allow_negative_scale; }

private:
    bool EnsureGizmo();
    bool resource_registered = false;
    bool allow_negative_scale = true;
};

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
