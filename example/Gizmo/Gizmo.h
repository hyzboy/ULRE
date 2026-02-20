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

struct GizmoMoveECS;
struct GizmoRotateECS;
struct GizmoScaleECS;

// 统一 Gizmo 世界（推荐使用）
class RenderPass;
struct GizmoECS;

enum class GizmoMode : int
{
    MoveWorld = 1,
    MoveLocal = 2,
    Rotate = 3,
    ScaleLocal = 4,

    Move = MoveWorld,
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

struct GizmoMoveECSState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_dist = 0.0f;
    float pick_dist = 0.0f;
};

struct GizmoRotateECSState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_angle = 0.0f;
    float pick_angle = 0.0f;
};

struct GizmoScaleECSState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_scale = 1.0f;
    float pick_scale = 1.0f;
    float cur_scale_u = 1.0f;
    float cur_scale_v = 1.0f;
    float pick_scale_u = 1.0f;
    float pick_scale_v = 1.0f;
};

GizmoMoveECS *CreateGizmoMoveECS(::hgl::ecs::World *world,
                                 const char *name,
                                 const math::Vector3f &position);
void DestroyGizmoMoveECS(GizmoMoveECS *gizmo);
bool GetGizmoMoveECSState(const GizmoMoveECS *gizmo, GizmoMoveECSState &out_state);
void UpdateGizmoMoveECS(GizmoMoveECS *gizmo,
                        const math::Vector2i &mouse_coord,
                        const CameraInfo *camera_info,
                        const ViewportInfo *viewport_info,
                        ::hgl::ecs::InputSystem *input_system,
                        bool left_down,
                        bool left_pressed,
                        bool left_released);

GizmoRotateECS *CreateGizmoRotateECS(::hgl::ecs::World *world,
                                      const char *name,
                                      const math::Vector3f &position);
void DestroyGizmoRotateECS(GizmoRotateECS *gizmo);
bool GetGizmoRotateECSState(const GizmoRotateECS *gizmo, GizmoRotateECSState &out_state);
void UpdateGizmoRotateECS(GizmoRotateECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          ::hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);

GizmoScaleECS *CreateGizmoScaleECS(::hgl::ecs::World *world,
                                    const char *name,
                                    const math::Vector3f &position);
void DestroyGizmoScaleECS(GizmoScaleECS *gizmo);
bool GetGizmoScaleECSState(const GizmoScaleECS *gizmo, GizmoScaleECSState &out_state);
void UpdateGizmoScaleECS(GizmoScaleECS *gizmo,
                         const math::Vector2i &mouse_coord,
                         const CameraInfo *camera_info,
                         const ViewportInfo *viewport_info,
                         ::hgl::ecs::InputSystem *input_system,
                         bool left_down,
                         bool left_pressed,
                         bool left_released);

/// ============= Unified Gizmo Interface (Recommended) =============

GizmoECS *CreateGizmoECS(::hgl::ecs::ECSContext *world,
                         const char *name,
                         const math::Vector3f &position);
GizmoECS *CreateDefaultGizmo(::hgl::ecs::ECSContext *world,
                             const char *name,
                             const math::Vector3f &position,
                             GizmoMode default_mode = GizmoMode::MoveWorld);
void DestroyGizmoECS(GizmoECS *gizmo);

void SetGizmoMode(GizmoECS *gizmo, GizmoMode mode);
GizmoMode GetGizmoMode(const GizmoECS *gizmo);

void SetGizmoVisible(GizmoECS *gizmo, bool visible);
hgl::ecs::Entity *GetGizmoRootEntity(const GizmoECS *gizmo);
bool BindGizmoTargetEntity(GizmoECS *gizmo, hgl::ecs::Entity *target_entity);
hgl::ecs::Entity *GetGizmoTargetEntity(const GizmoECS *gizmo);
void SetGizmoChangedCallback(GizmoECS *gizmo, GizmoChangedCallback callback);
void SetGizmoAllowNegativeScale(GizmoECS *gizmo, bool enabled);
bool IsGizmoAllowNegativeScale(const GizmoECS *gizmo);

class GizmoSystem : public hgl::ecs::System
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

public:
    GizmoSystem();
    ~GizmoSystem() override;

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

    void SetInitialPosition(const math::Vector3f &position) { initial_position = position; }
    const math::Vector3f &GetInitialPosition() const { return initial_position; }

    void SetChangedCallback(GizmoChangedCallback callback);

    void SetAllowNegativeScale(bool enabled);
    bool IsAllowNegativeScale() const { return allow_negative_scale; }

    GizmoECS *GetGizmo() const { return gizmo; }

private:
    bool EnsureGizmo();
    bool resource_registered = false;
    bool allow_negative_scale = true;
};

class SunDirectionGizmoSystem : public hgl::ecs::System
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
    SunDirectionGizmoSystem();
    ~SunDirectionGizmoSystem() override;

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

void UpdateGizmoECS(GizmoECS *gizmo,
                    const math::Vector2i &mouse_coord,
                    const CameraInfo *camera_info,
                    const ViewportInfo *viewport_info,
                    ::hgl::ecs::InputSystem *input_system,
                    bool left_down,
                    bool left_pressed,
                    bool left_released);

void SetGizmoMoveVisible(GizmoMoveECS *gizmo, bool visible);
void SetGizmoRotateVisible(GizmoRotateECS *gizmo, bool visible);
void SetGizmoScaleVisible(GizmoScaleECS *gizmo, bool visible);

enum class GizmoColor:uint
{
    Black=0,
    White,

    Red,
    Green,
    Blue,

    Yellow,

    ENUM_CLASS_RANGE(Black,Yellow)
};

enum class GizmoShape:uint
{
    Square=0,   //方块
    Circle,     //圆
    Cube,       //立方体
    Sphere,     //球
    Cone,       //圆锥
    Cylinder,   //圆柱
    Torus,      //圆环

    ENUM_CLASS_RANGE(Square,Torus)
};

bool InitGizmoResource(GraphicsContext *, RenderPass *);
void FreeGizmoResource();
bool EnsureGizmoSystemResources(::hgl::ecs::ECSContext *world);
void ForceReleaseGizmoSystemResources();
bool IsGizmoSystemResourcesResident();

MaterialInstance *GetGizmoMI3D(const GizmoColor &);
Primitive *GetGizmoMeshPrimitive(const GizmoShape &shape);

}//namespace hgl::graph
