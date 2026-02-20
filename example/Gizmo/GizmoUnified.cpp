/*
 统一 Gizmo 架构 - 通过 SubWorldComponent 管理三个 Gizmo 子世界

 结构：
     Main World
         └── GizmoECS (root)
                ├── Move (SubWorld)
                ├── Rotate (SubWorld)
                └── Scale (SubWorld)
*/

#include"Gizmo.h"
#include"GizmoInternal.h"
#include"GizmoResource.h"
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/World.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<glm/gtc/quaternion.hpp>
#include<glm/geometric.hpp>
#include<iostream>
#include<utility>

namespace hgl::graph{

namespace
{
    struct GizmoSystemResidentState
    {
        bool resources_ready = false;
        bool standby = false;
        uint32_t active_system_count = 0;
    };

    GizmoSystemResidentState g_gizmo_resident_state;
}

struct GizmoECS
{
    hgl::ecs::ECSContext* world = nullptr;
    hgl::ecs::Entity* root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;

    // 保存各个 Gizmo 的内部指针（内部实现使用）
    void* move_impl = nullptr;     // MoveGizmoImpl*
    void* rotate_impl = nullptr;   // RotateGizmoImpl*
    void* scale_impl = nullptr;    // ScaleGizmoImpl*

    // 对应的 SubWorld 和 Entity
    hgl::ecs::Entity* move_entity = nullptr;
    hgl::ecs::Entity* rotate_entity = nullptr;
    hgl::ecs::Entity* scale_entity = nullptr;

    std::shared_ptr<hgl::ecs::SubWorldComponent> move_subworld;
    std::shared_ptr<hgl::ecs::SubWorldComponent> rotate_subworld;
    std::shared_ptr<hgl::ecs::SubWorldComponent> scale_subworld;

    hgl::ecs::World* move_world = nullptr;
    hgl::ecs::World* rotate_world = nullptr;
    hgl::ecs::World* scale_world = nullptr;

    float last_rotate_angle = 0.0f;
    float last_scale_value = 1.0f;
    float last_scale_value_u = 1.0f;
    float last_scale_value_v = 1.0f;
    float last_move_dist = 0.0f;
    int last_rotate_axis = -1;
    int last_scale_axis = -1;
    int last_move_axis = -1;

    GizmoMode current_mode = GizmoMode::MoveWorld;
    bool allow_negative_scale = true;

    hgl::ecs::Entity* target_entity = nullptr;
    GizmoChangedCallback on_changed;
};

// Legacy internal entry points (implementation body kept unchanged).
GizmoECS *CreateGizmoECS(hgl::ecs::ECSContext *world,
                         const char *name,
                         const math::Vector3f &position);
void DestroyTransformGizmo(GizmoECS *gizmo);
void SetTransformGizmoMode(GizmoECS *gizmo, GizmoMode mode);
GizmoMode GetTransformGizmoMode(const GizmoECS *gizmo);
void SetTransformGizmoVisible(GizmoECS *gizmo, bool visible);
bool BindTransformGizmoTargetEntity(GizmoECS *gizmo, hgl::ecs::Entity *target_entity);
hgl::ecs::Entity *GetTransformGizmoTargetEntity(const GizmoECS *gizmo);
void SetTransformGizmoChangedCallback(GizmoECS *gizmo, GizmoChangedCallback callback);
void SetTransformGizmoAllowNegativeScale(GizmoECS *gizmo, bool enabled);
bool IsTransformGizmoAllowNegativeScale(const GizmoECS *gizmo);
void UpdateTransformGizmo(GizmoECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);

static void SyncAllSubGizmoTransforms(GizmoECS *gizmo);

static bool IsNearlyEqual(const math::Vector3f &a, const math::Vector3f &b, float epsilon = 1e-5f)
{
    return glm::length(a - b) <= epsilon;
}

static bool IsNearlyEqualRotation(const glm::quat &a, const glm::quat &b, float epsilon = 1e-5f)
{
    const float d = std::fabs(glm::dot(a, b));
    return std::fabs(1.0f - d) <= epsilon;
}

static bool IsTransformChanged(const math::Vector3f &prev_pos,
                               const glm::quat &prev_rot,
                               const math::Vector3f &prev_scale,
                               const math::Vector3f &cur_pos,
                               const glm::quat &cur_rot,
                               const math::Vector3f &cur_scale)
{
    if(!IsNearlyEqual(prev_pos, cur_pos))
        return true;

    if(!IsNearlyEqualRotation(prev_rot, cur_rot))
        return true;

    if(!IsNearlyEqual(prev_scale, cur_scale))
        return true;

    return false;
}

static void NormalizeScaleByPolicy(glm::vec3 &scale, bool allow_negative_scale)
{
    if (!allow_negative_scale)
        scale = glm::abs(scale);

    auto clamp_component = [](float value)
    {
        if (std::fabs(value) < 0.05f)
            return (value < 0.0f) ? -0.05f : 0.05f;
        return value;
    };

    scale.x = clamp_component(scale.x);
    scale.y = clamp_component(scale.y);
    scale.z = clamp_component(scale.z);
}

static void ApplyScalePolicyToTargetIfNeeded(GizmoECS *gizmo)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    glm::vec3 scale = gizmo->root_transform->GetLocalScale();
    const glm::vec3 original_scale = scale;
    NormalizeScaleByPolicy(scale, gizmo->allow_negative_scale);

    if (glm::length(scale - original_scale) > 1e-6f)
    {
        gizmo->root_transform->SetLocalScale(scale);

        if (gizmo->target_entity)
        {
            auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
            if (target_transform)
                target_transform->SetLocalScale(scale);
        }

        SyncAllSubGizmoTransforms(gizmo);
    }
}

static void SyncAllSubGizmoTransforms(GizmoECS *gizmo)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    const math::Vector3f root_pos = gizmo->root_transform->GetLocalPosition();
    const glm::quat root_rot = gizmo->root_transform->GetLocalRotation();

    SetMoveGizmoPosition((MoveGizmoImpl*)gizmo->move_impl, root_pos);
    SetMoveGizmoRotation((MoveGizmoImpl*)gizmo->move_impl,
                         gizmo->current_mode == GizmoMode::MoveLocal ? root_rot : glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    SetRotateGizmoPosition((RotateGizmoImpl*)gizmo->rotate_impl, root_pos);
    SetRotateGizmoRotation((RotateGizmoImpl*)gizmo->rotate_impl,
                          gizmo->current_mode == GizmoMode::RotateLocal ? root_rot : glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    SetScaleGizmoPosition((ScaleGizmoImpl*)gizmo->scale_impl, root_pos);
    SetScaleGizmoRotation((ScaleGizmoImpl*)gizmo->scale_impl, root_rot);
}

static bool IsCurrentModeDragging(const GizmoECS *gizmo)
{
    if(!gizmo)
        return false;

    switch(gizmo->current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
        {
            MoveGizmoInteractionState state;
            return GetMoveGizmoInteractionState((const MoveGizmoImpl*)gizmo->move_impl, state) && state.dragging;
        }
    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
        {
            RotateGizmoInteractionState state;
            return GetRotateGizmoInteractionState((const RotateGizmoImpl*)gizmo->rotate_impl, state) && state.dragging;
        }
    case GizmoMode::ScaleLocal:
        {
            ScaleGizmoInteractionState state;
            return GetScaleGizmoInteractionState((const ScaleGizmoImpl*)gizmo->scale_impl, state) && state.dragging;
        }
    }

    return false;
}


GizmoECS *CreateTransformGizmo(hgl::ecs::ECSContext *world,
                               const char *name,
                               const math::Vector3f &position)
{
    if (!world)
        return nullptr;

    auto *gizmo = new GizmoECS;
    gizmo->world = world;
    std::cout << "[GizmoECS] Create begin name=" << (name ? name : "Gizmo") << std::endl;

    // Create root entity for entire Gizmo
    gizmo->root = world->CreateEntity<hgl::ecs::Entity>(name ? name : "Gizmo");
    if (!gizmo->root)
    {
        std::cout << "[GizmoECS] Create root entity failed" << std::endl;
        delete gizmo;
        return nullptr;
    }
    std::cout << "[GizmoECS] Root entity id=" << gizmo->root->GetID().index << std::endl;

    gizmo->root_transform = gizmo->root->AddComponent<hgl::ecs::TransformComponent>();
    gizmo->root_transform->SetLocalTRS(glm::vec3(position), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    gizmo->root_transform->SetMovable(true);

    // Create three child entities with SubWorldComponent for each Gizmo mode

    // Move Gizmo
    {
        gizmo->move_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Move");
        if (!gizmo->move_entity)
        {
            std::cout << "[GizmoECS] Create move entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto move_transform = gizmo->move_entity->AddComponent<hgl::ecs::TransformComponent>();
        move_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        move_transform->SetParent(gizmo->root->GetID());

        auto sub_world = gizmo->move_entity->AddComponent<hgl::ecs::SubWorldComponent>();
        gizmo->move_subworld = sub_world;
        gizmo->move_world = sub_world->GetSubWorld();

        if (!gizmo->move_world)
        {
            std::cout << "[GizmoECS] Move subworld is null" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        gizmo->move_impl = (void*)CreateMoveGizmoImpl(gizmo->move_world, "GizmoMove", math::Vector3f(0, 0, 0));
        if (!gizmo->move_impl)
        {
            std::cout << "[GizmoECS] Create move gizmo failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }
    }

    // Rotate Gizmo
    {
        gizmo->rotate_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Rotate");
        if (!gizmo->rotate_entity)
        {
            std::cout << "[GizmoECS] Create rotate entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto rotate_transform = gizmo->rotate_entity->AddComponent<hgl::ecs::TransformComponent>();
        rotate_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        rotate_transform->SetParent(gizmo->root->GetID());

        auto sub_world = gizmo->rotate_entity->AddComponent<hgl::ecs::SubWorldComponent>();
        gizmo->rotate_subworld = sub_world;
        gizmo->rotate_world = sub_world->GetSubWorld();

        if (!gizmo->rotate_world)
        {
            std::cout << "[GizmoECS] Rotate subworld is null" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        gizmo->rotate_impl = (void*)CreateRotateGizmoImpl(gizmo->rotate_world, "GizmoRotate", math::Vector3f(0, 0, 0));
        if (!gizmo->rotate_impl)
        {
            std::cout << "[GizmoECS] Create rotate gizmo failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }
    }

    // Scale Gizmo
    {
        gizmo->scale_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Scale");
        if (!gizmo->scale_entity)
        {
            std::cout << "[GizmoECS] Create scale entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto scale_transform = gizmo->scale_entity->AddComponent<hgl::ecs::TransformComponent>();
        scale_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        scale_transform->SetParent(gizmo->root->GetID());

        auto sub_world = gizmo->scale_entity->AddComponent<hgl::ecs::SubWorldComponent>();
        gizmo->scale_subworld = sub_world;
        gizmo->scale_world = sub_world->GetSubWorld();

        if (!gizmo->scale_world)
        {
            std::cout << "[GizmoECS] Scale subworld is null" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        gizmo->scale_impl = (void*)CreateScaleGizmoImpl(gizmo->scale_world, "GizmoScale", math::Vector3f(0, 0, 0));
        if (!gizmo->scale_impl)
        {
            std::cout << "[GizmoECS] Create scale gizmo failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }
    }

    // Initialize with Move mode active
    SetTransformGizmoMode(gizmo, GizmoMode::MoveWorld);
    SyncAllSubGizmoTransforms(gizmo);
    std::cout << "[GizmoECS] Create done" << std::endl;

    return gizmo;
}

GizmoECS *CreateDefaultTransformGizmo(hgl::ecs::ECSContext *world,
                                      const char *name,
                                      const math::Vector3f &position,
                                      GizmoMode default_mode)
{
    GizmoECS *gizmo = CreateTransformGizmo(world, name, position);
    if(!gizmo)
        return nullptr;

    SetTransformGizmoMode(gizmo, default_mode);
    SetTransformGizmoVisible(gizmo, true);
    return gizmo;
}

void DestroyTransformGizmo(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    std::cout << "[GizmoECS] Destroy begin" << std::endl;

    if (gizmo->move_impl)
    {
        DestroyMoveGizmoImpl((MoveGizmoImpl*)gizmo->move_impl);
    }

    if (gizmo->rotate_impl)
    {
        DestroyRotateGizmoImpl((RotateGizmoImpl*)gizmo->rotate_impl);
    }

    if (gizmo->scale_impl)
    {
        DestroyScaleGizmoImpl((ScaleGizmoImpl*)gizmo->scale_impl);
    }

    if (gizmo->world)
    {
        if (gizmo->move_entity)
            gizmo->world->DestroyEntity(gizmo->move_entity->GetID());
        if (gizmo->rotate_entity)
            gizmo->world->DestroyEntity(gizmo->rotate_entity->GetID());
        if (gizmo->scale_entity)
            gizmo->world->DestroyEntity(gizmo->scale_entity->GetID());
        if (gizmo->root)
            gizmo->world->DestroyEntity(gizmo->root->GetID());
    }

    delete gizmo;
    std::cout << "[GizmoECS] Destroy done" << std::endl;
}

void SetTransformGizmoMode(GizmoECS *gizmo, GizmoMode mode)
{
    if (!gizmo)
        return;

    gizmo->current_mode = mode;
    std::cout << "[GizmoECS] Set mode=" << static_cast<int>(mode) << std::endl;

    // Set visibility based on mode
    const bool move_active = (mode == GizmoMode::MoveWorld || mode == GizmoMode::MoveLocal);
    const bool rotate_active = (mode == GizmoMode::RotateWorld || mode == GizmoMode::RotateLocal);

    SetMoveGizmoVisible((MoveGizmoImpl*)gizmo->move_impl, move_active);
    SetRotateGizmoVisible((RotateGizmoImpl*)gizmo->rotate_impl, rotate_active);
    SetScaleGizmoVisible((ScaleGizmoImpl*)gizmo->scale_impl, mode == GizmoMode::ScaleLocal);

    // Pause non-active sub-worlds to avoid concurrent rendering/update artifacts
    if (gizmo->move_subworld)
        gizmo->move_subworld->SetPaused(!move_active);
    if (gizmo->rotate_subworld)
        gizmo->rotate_subworld->SetPaused(!rotate_active);
    if (gizmo->scale_subworld)
        gizmo->scale_subworld->SetPaused(mode != GizmoMode::ScaleLocal);

    gizmo->last_rotate_angle = 0.0f;
    gizmo->last_scale_value = 1.0f;
    gizmo->last_scale_value_u = 1.0f;
    gizmo->last_scale_value_v = 1.0f;
    gizmo->last_move_dist = 0.0f;
    gizmo->last_rotate_axis = -1;
    gizmo->last_scale_axis = -1;
    gizmo->last_move_axis = -1;

    SyncAllSubGizmoTransforms(gizmo);
}

GizmoMode GetTransformGizmoMode(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->current_mode : GizmoMode::MoveWorld;
}

void SetTransformGizmoVisible(GizmoECS *gizmo, bool visible)
{
    if (!gizmo || !gizmo->root)
        return;

    auto vis_comp = gizmo->root->GetComponent<hgl::ecs::VisibilityComponent>();
    if (!vis_comp)
    {
        vis_comp = gizmo->root->AddComponent<hgl::ecs::VisibilityComponent>();
    }

    if (vis_comp)
    {
        vis_comp->SetVisible(visible);
        std::cout << "[GizmoECS] Set root visible=" << (visible ? 1 : 0) << std::endl;
    }
}

static hgl::ecs::Entity *GetGizmoRootEntity(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->root : nullptr;
}

bool BindTransformGizmoTargetEntity(GizmoECS *gizmo, hgl::ecs::Entity *target_entity)
{
    if(!gizmo)
        return false;

    gizmo->target_entity = target_entity;

    if(!target_entity || !gizmo->root_transform)
        return true;

    auto target_transform = target_entity->GetComponent<hgl::ecs::TransformComponent>();
    if(!target_transform)
        return false;

    gizmo->root_transform->SetLocalTRS(target_transform->GetLocalPosition(),
                                       target_transform->GetLocalRotation(),
                                       target_transform->GetLocalScale());
    SyncAllSubGizmoTransforms(gizmo);

    return true;
}

hgl::ecs::Entity *GetTransformGizmoTargetEntity(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->target_entity : nullptr;
}

void SetTransformGizmoChangedCallback(GizmoECS *gizmo, GizmoChangedCallback callback)
{
    if(!gizmo)
        return;

    gizmo->on_changed = std::move(callback);
}

void SetTransformGizmoAllowNegativeScale(GizmoECS *gizmo, bool enabled)
{
    if (!gizmo)
        return;

    gizmo->allow_negative_scale = enabled;
    ApplyScalePolicyToTargetIfNeeded(gizmo);
}

bool IsTransformGizmoAllowNegativeScale(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->allow_negative_scale : true;
}

void UpdateTransformGizmo(GizmoECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                    hgl::ecs::InputSystem *input_system,
                    bool left_down,
                    bool left_pressed,
                    bool left_released)
{
    if (!gizmo)
        return;

    if(gizmo->target_entity && gizmo->root_transform && !IsCurrentModeDragging(gizmo))
    {
        auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
        if(target_transform)
        {
            gizmo->root_transform->SetLocalTRS(target_transform->GetLocalPosition(),
                                               target_transform->GetLocalRotation(),
                                               target_transform->GetLocalScale());
            SyncAllSubGizmoTransforms(gizmo);
        }
    }

    math::Vector3f prev_pos(0.0f);
    glm::quat prev_rot(1.0f, 0.0f, 0.0f, 0.0f);
    math::Vector3f prev_scale(1.0f);
    if(gizmo->root_transform)
    {
        prev_pos = gizmo->root_transform->GetLocalPosition();
        prev_rot = gizmo->root_transform->GetLocalRotation();
        prev_scale = gizmo->root_transform->GetLocalScale();
    }

    // Update only the active mode
    switch (gizmo->current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
    {
        UpdateMoveGizmoImpl((MoveGizmoImpl*)gizmo->move_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            math::Vector3f move_pos;
            if(GetMoveGizmoPosition((MoveGizmoImpl*)gizmo->move_impl, move_pos))
                gizmo->root_transform->SetLocalPosition(glm::vec3(move_pos));
        }
        break;
    }
    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
    {
        UpdateRotateGizmoImpl((RotateGizmoImpl*)gizmo->rotate_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            RotateGizmoInteractionState state;
            if(GetRotateGizmoInteractionState((RotateGizmoImpl*)gizmo->rotate_impl, state))
            {
                if(state.dragging && state.pick_axis >= 0 && state.pick_axis <= 3)
                {
                    if(gizmo->last_rotate_axis != state.pick_axis)
                    {
                        gizmo->last_rotate_axis = state.pick_axis;
                        gizmo->last_rotate_angle = state.cur_angle;
                    }

                    const float delta = state.cur_angle - gizmo->last_rotate_angle;
                    if(std::fabs(delta) > 1e-6f)
                    {
                        math::Vector3f axis;
                        if(state.pick_axis < 3)
                        {
                            // Get base axis in world space
                            axis = math::GetAxisVector(math::AXIS(state.pick_axis));
                            
                            // For local rotation, transform axis to local space
                            if(gizmo->current_mode == GizmoMode::RotateLocal)
                            {
                                const glm::quat cur_rotation = gizmo->root_transform->GetLocalRotation();
                                axis = glm::vec3(cur_rotation * glm::vec4(axis, 0.0f));
                            }
                        }
                        else if(camera_info)
                        {
                            axis = glm::normalize(math::Vector3f(camera_info->view[0][2], camera_info->view[1][2], camera_info->view[2][2]));
                        }
                        else
                        {
                            axis = math::AxisVector::Z;
                        }

                        const glm::quat dq = glm::angleAxis(delta, glm::vec3(axis));
                        const glm::quat cur = gizmo->root_transform->GetLocalRotation();
                        gizmo->root_transform->SetLocalRotation(glm::normalize(dq * cur));
                    }

                    gizmo->last_rotate_angle = state.cur_angle;
                }
                else
                {
                    gizmo->last_rotate_axis = -1;
                    gizmo->last_rotate_angle = 0.0f;
                }
            }
        }
        break;
    }
    case GizmoMode::ScaleLocal:
    {
        UpdateScaleGizmoImpl((ScaleGizmoImpl*)gizmo->scale_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            ScaleGizmoInteractionState state;
            if(GetScaleGizmoInteractionState((ScaleGizmoImpl*)gizmo->scale_impl, state))
            {
                if(state.dragging && state.pick_axis >= 0)
                {
                    if(gizmo->last_scale_axis != state.pick_axis)
                    {
                        gizmo->last_scale_axis = state.pick_axis;
                        gizmo->last_scale_value = state.cur_scale;
                        gizmo->last_scale_value_u = state.cur_scale_u;
                        gizmo->last_scale_value_v = state.cur_scale_v;
                    }

                    float base = gizmo->last_scale_value;
                    if(std::fabs(base) < 1e-6f)
                        base = 1.0f;

                    const float ratio = state.cur_scale / base;
                    if(state.pick_axis < 3)
                    {
                        if(std::fabs(ratio - 1.0f) > 1e-6f)
                        {
                            glm::vec3 cur = gizmo->root_transform->GetLocalScale();
                            cur[state.pick_axis] *= ratio;
                            NormalizeScaleByPolicy(cur, gizmo->allow_negative_scale);
                            gizmo->root_transform->SetLocalScale(cur);
                        }
                    }
                    else if(state.pick_axis < 6)
                    {
                        float base_u = gizmo->last_scale_value_u;
                        float base_v = gizmo->last_scale_value_v;

                        if(std::fabs(base_u) < 1e-6f)
                            base_u = 1.0f;
                        if(std::fabs(base_v) < 1e-6f)
                            base_v = 1.0f;

                        const float ratio_u = state.cur_scale_u / base_u;
                        const float ratio_v = state.cur_scale_v / base_v;

                        if(std::fabs(ratio_u - 1.0f) > 1e-6f || std::fabs(ratio_v - 1.0f) > 1e-6f)
                        {
                            glm::vec3 cur = gizmo->root_transform->GetLocalScale();
                            static const int plane_axes[3][2] =
                            {
                                {1, 2}, // YZ
                                {0, 2}, // XZ
                                {0, 1}  // XY
                            };

                            const int plane_index = state.pick_axis - 3;
                            cur[plane_axes[plane_index][0]] *= ratio_u;
                            cur[plane_axes[plane_index][1]] *= ratio_v;

                            NormalizeScaleByPolicy(cur, gizmo->allow_negative_scale);
                            gizmo->root_transform->SetLocalScale(cur);
                        }
                    }

                    gizmo->last_scale_value = state.cur_scale;
                    gizmo->last_scale_value_u = state.cur_scale_u;
                    gizmo->last_scale_value_v = state.cur_scale_v;
                }
                else
                {
                    gizmo->last_scale_axis = -1;
                    gizmo->last_scale_value = 1.0f;
                    gizmo->last_scale_value_u = 1.0f;
                    gizmo->last_scale_value_v = 1.0f;
                }
            }
        }
        break;
    }
    }

    SyncAllSubGizmoTransforms(gizmo);

    if(gizmo->root_transform)
    {
        const math::Vector3f cur_pos = gizmo->root_transform->GetLocalPosition();
        const glm::quat cur_rot = gizmo->root_transform->GetLocalRotation();
        const math::Vector3f cur_scale = gizmo->root_transform->GetLocalScale();
        const bool changed = IsTransformChanged(prev_pos, prev_rot, prev_scale,
                                                cur_pos, cur_rot, cur_scale);

        if(gizmo->target_entity)
        {
            auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
            if(target_transform)
            {
                if(IsTransformChanged(target_transform->GetLocalPosition(),
                                      target_transform->GetLocalRotation(),
                                      target_transform->GetLocalScale(),
                                      cur_pos,
                                      cur_rot,
                                      cur_scale))
                {
                    target_transform->SetLocalTRS(cur_pos, cur_rot, cur_scale);
                }
            }
        }

        if(changed && gizmo->on_changed)
        {
            GizmoTransformChange change;
            change.previous_position = prev_pos;
            change.current_position = cur_pos;
            change.previous_rotation = prev_rot;
            change.current_rotation = cur_rot;
            change.previous_scale = prev_scale;
            change.current_scale = cur_scale;
            change.mode = gizmo->current_mode;
            gizmo->on_changed(change);
        }
    }
}

bool EnsureGizmoSystemResources(hgl::ecs::ECSContext *world)
{
    if (g_gizmo_resident_state.resources_ready)
    {
        g_gizmo_resident_state.standby = false;
        return true;
    }

    if (!world)
        return false;

    auto *graphics = world->GetGraphicsContext();
    auto *render_context = world->GetRenderContext();
    auto *render_target = render_context ? render_context->GetCurrentRenderTarget() : nullptr;
    auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;

    if (!graphics || !render_pass)
        return false;

    if (!InitGizmoResource(graphics, render_pass))
        return false;

    g_gizmo_resident_state.resources_ready = true;
    g_gizmo_resident_state.standby = false;
    return true;
}

void ForceReleaseGizmoSystemResources()
{
    if (!g_gizmo_resident_state.resources_ready)
        return;

    FreeGizmoResource();
    g_gizmo_resident_state.resources_ready = false;
    g_gizmo_resident_state.standby = false;
}

bool IsGizmoSystemResourcesResident()
{
    return g_gizmo_resident_state.resources_ready;
}

namespace
{
    glm::quat DirectionToRotation(const math::Vector3f &dir)
    {
        const float len2 = glm::dot(dir, dir);
        const math::Vector3f forward = (len2 > 1e-8f) ? glm::normalize(dir) : math::AxisVector::Z;

        const math::Vector3f world_up = math::AxisVector::Y;
        math::Vector3f right = glm::cross(world_up, forward);
        if (glm::dot(right, right) < 1e-8f)
        {
            const math::Vector3f fallback_up = math::AxisVector::X;
            right = glm::cross(fallback_up, forward);
        }

        right = glm::normalize(right);
        const math::Vector3f up = glm::normalize(glm::cross(forward, right));

        glm::mat3 basis(1.0f);
        basis[0] = right;
        basis[1] = up;
        basis[2] = forward;
        return glm::normalize(glm::quat_cast(basis));
    }

    math::Vector3f RotationToDirection(const glm::quat &rot)
    {
        const math::Vector3f forward = glm::normalize(rot * math::AxisVector::Z);
        return forward;
    }
}

TransformGizmoSystem::TransformGizmoSystem()
    : hgl::ecs::System("TransformGizmoSystem")
{
    SetExecutionOrder(hgl::ecs::ExecutionPhase::TickCamera, hgl::ecs::ExecutionPriority::Last);
    AddDependency<hgl::ecs::InputSystem>();
    AddDependency<hgl::ecs::CameraSystem>();
}

TransformGizmoSystem::~TransformGizmoSystem()
{
    Shutdown();
}

void TransformGizmoSystem::Initialize()
{
    if (!resource_registered)
    {
        ++g_gizmo_resident_state.active_system_count;
        resource_registered = true;
    }

    EnsureGizmoSystemResources(context);
    EnsureGizmo();
}

void TransformGizmoSystem::Shutdown()
{
    if (gizmo)
    {
        DestroyTransformGizmo(gizmo);
        gizmo = nullptr;
    }

    if (resource_registered)
    {
        if (g_gizmo_resident_state.active_system_count > 0)
            --g_gizmo_resident_state.active_system_count;

        resource_registered = false;
    }

    if (g_gizmo_resident_state.active_system_count == 0 && g_gizmo_resident_state.resources_ready)
    {
        g_gizmo_resident_state.standby = true;
    }
}

bool TransformGizmoSystem::EnsureGizmo()
{
    if (gizmo)
        return true;

    if (!context)
        return false;

    if (!EnsureGizmoSystemResources(context))
        return false;

    math::Vector3f create_pos = initial_position;
    if (target_entity)
    {
        auto target_transform = target_entity->GetComponent<hgl::ecs::TransformComponent>();
        if (target_transform)
            create_pos = target_transform->GetLocalPosition();
    }

    gizmo = CreateDefaultTransformGizmo(context, "Gizmo", create_pos, default_mode);
    if (!gizmo)
        return false;

    if (target_entity)
        BindTransformGizmoTargetEntity(gizmo, target_entity);

    SetTransformGizmoAllowNegativeScale(gizmo, allow_negative_scale);

    if (changed_callback)
        SetTransformGizmoChangedCallback(gizmo, changed_callback);

    return true;
}

bool TransformGizmoSystem::SetTargetEntity(hgl::ecs::Entity *entity)
{
    target_entity = entity;

    if (!gizmo)
        return true;

    return BindTransformGizmoTargetEntity(gizmo, target_entity);
}

bool TransformGizmoSystem::SetTargetTransform(const std::shared_ptr<hgl::ecs::TransformComponent> &transform)
{
    if (!transform)
        return SetTargetEntity(nullptr);

    return SetTargetEntity(transform->GetOwner());
}

void TransformGizmoSystem::SetChangedCallback(GizmoChangedCallback callback)
{
    changed_callback = std::move(callback);
    if (gizmo)
        SetTransformGizmoChangedCallback(gizmo, changed_callback);
}

void TransformGizmoSystem::SetAllowNegativeScale(bool enabled)
{
    allow_negative_scale = enabled;
    if (gizmo)
        SetTransformGizmoAllowNegativeScale(gizmo, enabled);
}

GizmoMode TransformGizmoSystem::GetCurrentMode() const
{
    if(!gizmo)
        return default_mode;

    return GetTransformGizmoMode(gizmo);
}

void TransformGizmoSystem::Update(float)
{
    if (!context)
        return;

    if (!IsGizmoSystemResourcesResident())
    {
        if (gizmo)
        {
            DestroyTransformGizmo(gizmo);
            gizmo = nullptr;
        }

        if (!EnsureGizmoSystemResources(context))
            return;
    }

    if (!EnsureGizmo())
        return;

    auto input_system = context->GetSystem<hgl::ecs::InputSystem>();
    auto camera_system = context->GetSystem<hgl::ecs::CameraSystem>();
    if (!input_system || !camera_system)
        return;

    const CameraInfo *camera_info = camera_system->GetCameraInfo();
    const ViewportInfo *viewport_info = camera_system->GetViewportInfo();
    if (!camera_info || !viewport_info)
        return;

    const math::Vector2i mouse_coord = input_system->GetMouseCoord();
    const bool left_down = input_system->IsMouseButtonDown(hgl::io::MouseButton::Left);
    const bool left_pressed = left_down && !last_left_down;
    const bool left_released = !left_down && last_left_down;
    last_left_down = left_down;

    if (mode_switch_enabled)
    {
        const bool key_1 = input_system->IsKeyDown(hgl::io::KeyboardButton::_1);
        const bool key_2 = input_system->IsKeyDown(hgl::io::KeyboardButton::_2);
        const bool key_3 = input_system->IsKeyDown(hgl::io::KeyboardButton::_3);
        const bool key_4 = input_system->IsKeyDown(hgl::io::KeyboardButton::_4);
        const bool key_5 = input_system->IsKeyDown(hgl::io::KeyboardButton::_5);

        if (key_1 && !last_key_1)
            SetTransformGizmoMode(gizmo, GizmoMode::MoveWorld);
        else if (key_2 && !last_key_2)
            SetTransformGizmoMode(gizmo, GizmoMode::MoveLocal);
        else if (key_3 && !last_key_3)
            SetTransformGizmoMode(gizmo, GizmoMode::RotateWorld);
        else if (key_4 && !last_key_4)
            SetTransformGizmoMode(gizmo, GizmoMode::RotateLocal);
        else if (key_5 && !last_key_5)
            SetTransformGizmoMode(gizmo, GizmoMode::ScaleLocal);

        last_key_1 = key_1;
        last_key_2 = key_2;
        last_key_3 = key_3;
        last_key_4 = key_4;
        last_key_5 = key_5;
    }

    UpdateTransformGizmo(gizmo,
                   mouse_coord,
                   camera_info,
                   viewport_info,
                   input_system.get(),
                   left_down,
                   left_pressed,
                   left_released);
}

SunDirectionControlSystem::SunDirectionControlSystem()
    : hgl::ecs::System("SunDirectionControlSystem")
{
    SetExecutionOrder(hgl::ecs::ExecutionPhase::TickCamera, hgl::ecs::ExecutionPriority::Last);
    AddDependency<hgl::ecs::InputSystem>();
    AddDependency<hgl::ecs::CameraSystem>();
}

SunDirectionControlSystem::~SunDirectionControlSystem()
{
    Shutdown();
}

bool SunDirectionControlSystem::EnsureEnvironment()
{
    if (!context)
        return false;

    if (environment_system)
        return true;

    if (!auto_find_environment)
        return false;

    auto env_sp = context->GetSystem<hgl::ecs::EnvironmentSystem>();
    environment_system = env_sp.get();
    return environment_system != nullptr;
}

bool SunDirectionControlSystem::EnsureProxyEntity()
{
    if (!context)
        return false;

    if (proxy_entity && proxy_transform)
        return true;

    proxy_entity = context->CreateEntity<hgl::ecs::Entity>("SunDirectionGizmoProxy");
    if (!proxy_entity)
        return false;

    proxy_transform = proxy_entity->AddComponent<hgl::ecs::TransformComponent>();
    if (!proxy_transform)
        return false;

    proxy_transform->SetMovable(true);
    proxy_transform->SetLocalTRS(glm::vec3(gizmo_position),
                                 glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                 glm::vec3(1.0f));
    return true;
}

bool SunDirectionControlSystem::EnsureGizmo()
{
    if (gizmo)
        return true;

    if (!context || !proxy_transform)
        return false;

    gizmo = CreateDefaultTransformGizmo(context, "SunDirectionGizmo", gizmo_position, GizmoMode::RotateWorld);
    if (!gizmo)
        return false;

    BindTransformGizmoTargetEntity(gizmo, proxy_entity);
    SetTransformGizmoMode(gizmo, GizmoMode::RotateWorld);
    SetTransformGizmoAllowNegativeScale(gizmo, false);
    return true;
}

void SunDirectionControlSystem::Initialize()
{
    if (!resource_registered)
    {
        ++g_gizmo_resident_state.active_system_count;
        resource_registered = true;
    }

    if (!EnsureGizmoSystemResources(context))
        return;

    if (!EnsureEnvironment())
        return;

    if (!EnsureProxyEntity())
        return;

    if (!EnsureGizmo())
        return;

    if (environment_system)
    {
        if (const auto *sky = environment_system->GetSkyInfo())
        {
            const math::Vector3f sun_dir(sky->sun_direction.x,
                                         sky->sun_direction.y,
                                         sky->sun_direction.z);

            proxy_transform->SetLocalPosition(glm::vec3(gizmo_position));
            proxy_transform->SetLocalRotation(DirectionToRotation(sun_dir));
        }
    }
}

void SunDirectionControlSystem::Shutdown()
{
    if (gizmo)
    {
        DestroyTransformGizmo(gizmo);
        gizmo = nullptr;
    }

    if (context && proxy_entity)
    {
        context->DestroyEntity(proxy_entity->GetID());
    }

    proxy_transform.reset();
    proxy_entity = nullptr;

    if (resource_registered)
    {
        if (g_gizmo_resident_state.active_system_count > 0)
            --g_gizmo_resident_state.active_system_count;
        resource_registered = false;
    }

    if (g_gizmo_resident_state.active_system_count == 0 && g_gizmo_resident_state.resources_ready)
        g_gizmo_resident_state.standby = true;
}

void SunDirectionControlSystem::SetGizmoVisible(bool visible)
{
    if (gizmo)
        hgl::graph::SetTransformGizmoVisible(gizmo, visible);
}

void SunDirectionControlSystem::Update(float)
{
    if (!context)
        return;

    if (!IsGizmoSystemResourcesResident())
    {
        if (gizmo)
        {
            DestroyTransformGizmo(gizmo);
            gizmo = nullptr;
        }

        if (!EnsureGizmoSystemResources(context))
            return;
    }

    if (!EnsureEnvironment())
        return;

    if (!EnsureProxyEntity())
        return;

    if (!EnsureGizmo())
        return;

    auto input_system = context->GetSystem<hgl::ecs::InputSystem>();
    auto camera_system = context->GetSystem<hgl::ecs::CameraSystem>();
    if (!input_system || !camera_system)
        return;

    const CameraInfo *camera_info = camera_system->GetCameraInfo();
    const ViewportInfo *viewport_info = camera_system->GetViewportInfo();
    if (!camera_info || !viewport_info)
        return;

    const math::Vector2i mouse_coord = input_system->GetMouseCoord();
    const bool left_down = input_system->IsMouseButtonDown(hgl::io::MouseButton::Left);
    const bool left_pressed = left_down && !last_left_down;
    const bool left_released = !left_down && last_left_down;
    last_left_down = left_down;

    if (GetTransformGizmoMode(gizmo) != GizmoMode::RotateWorld && GetTransformGizmoMode(gizmo) != GizmoMode::RotateLocal)
        SetTransformGizmoMode(gizmo, GizmoMode::RotateWorld);

    UpdateTransformGizmo(gizmo,
                   mouse_coord,
                   camera_info,
                   viewport_info,
                   input_system.get(),
                   left_down,
                   left_pressed,
                   left_released);

    if (environment_system)
    {
        auto *sky = environment_system->EditSkyInfo();
        if (sky)
        {
            if (proxy_transform)
                proxy_transform->SetLocalPosition(glm::vec3(gizmo_position));

            math::Vector3f dir(0.0f, 0.0f, 1.0f);
            if (auto *gizmo_root = GetGizmoRootEntity(gizmo))
            {
                auto gizmo_root_transform = gizmo_root->GetComponent<hgl::ecs::TransformComponent>();
                if (gizmo_root_transform)
                {
                    dir = RotationToDirection(gizmo_root_transform->GetLocalRotation());

                    if (proxy_transform)
                        proxy_transform->SetLocalRotation(gizmo_root_transform->GetLocalRotation());
                }
            }

            sky->sun_direction = math::Vector4f(dir.x, dir.y, dir.z, 0.0f);
            environment_system->MarkSkyDirty();
        }
    }
}

}//namespace hgl::graph




