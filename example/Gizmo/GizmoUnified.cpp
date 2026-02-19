/*
 统一 Gizmo 世界 - 通过 SubWorldComponent 管理三个 Gizmo 子世界

 结构：
   Main World
     └─ GizmoECS (root)
        ├─ Move (SubWorld)
        ├─ Rotate (SubWorld)
        └─ Scale (SubWorld)
*/

#include"Gizmo.h"
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/World.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<glm/gtc/quaternion.hpp>
#include<iostream>

namespace hgl::graph{

struct GizmoECS
{
    hgl::ecs::ECSContext* world = nullptr;
    hgl::ecs::Entity* root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;

    // 三个子 Gizmo 的管理指针（内部实现使用）
    void* move_impl = nullptr;     // GizmoMoveECS*
    void* rotate_impl = nullptr;   // GizmoRotateECS*
    void* scale_impl = nullptr;    // GizmoScaleECS*

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
    int last_rotate_axis = -1;
    int last_scale_axis = -1;

    GizmoMode current_mode = GizmoMode::Move;
};

// Forward declare the internal gizmo functions
extern GizmoMoveECS *CreateGizmoMoveECS(hgl::ecs::World *world,
                                        const char *name,
                                        const math::Vector3f &position);
extern void DestroyGizmoMoveECS(GizmoMoveECS *gizmo);
extern void UpdateGizmoMoveECS(GizmoMoveECS *gizmo,
                                const math::Vector2i &mouse_coord,
                                const CameraInfo *camera_info,
                                const ViewportInfo *viewport_info,
                                hgl::ecs::InputSystem *input_system,
                                bool left_down,
                                bool left_pressed,
                                bool left_released);
extern bool GetGizmoMovePosition(const GizmoMoveECS *gizmo, math::Vector3f &out_position);
extern void SetGizmoMovePosition(GizmoMoveECS *gizmo, const math::Vector3f &position);

extern GizmoRotateECS *CreateGizmoRotateECS(hgl::ecs::World *world,
                                            const char *name,
                                            const math::Vector3f &position);
extern void DestroyGizmoRotateECS(GizmoRotateECS *gizmo);
extern void UpdateGizmoRotateECS(GizmoRotateECS *gizmo,
                                    const math::Vector2i &mouse_coord,
                                    const CameraInfo *camera_info,
                                    const ViewportInfo *viewport_info,
                                    hgl::ecs::InputSystem *input_system,
                                    bool left_down,
                                    bool left_pressed,
                                    bool left_released);
extern bool GetGizmoRotateECSState(const GizmoRotateECS *gizmo, GizmoRotateECSState &out_state);
extern void SetGizmoRotatePosition(GizmoRotateECS *gizmo, const math::Vector3f &position);

extern GizmoScaleECS *CreateGizmoScaleECS(hgl::ecs::World *world,
                                            const char *name,
                                            const math::Vector3f &position);
extern void DestroyGizmoScaleECS(GizmoScaleECS *gizmo);
extern void UpdateGizmoScaleECS(GizmoScaleECS *gizmo,
                                const math::Vector2i &mouse_coord,
                                const CameraInfo *camera_info,
                                const ViewportInfo *viewport_info,
                                hgl::ecs::InputSystem *input_system,
                                bool left_down,
                                bool left_pressed,
                                bool left_released);
extern bool GetGizmoScaleECSState(const GizmoScaleECS *gizmo, GizmoScaleECSState &out_state);
extern void SetGizmoScalePosition(GizmoScaleECS *gizmo, const math::Vector3f &position);

extern void SetGizmoMoveVisible(GizmoMoveECS *gizmo, bool visible);
extern void SetGizmoRotateVisible(GizmoRotateECS *gizmo, bool visible);
extern void SetGizmoScaleVisible(GizmoScaleECS *gizmo, bool visible);

static void SyncAllSubGizmoPositions(GizmoECS *gizmo)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    const math::Vector3f root_pos = gizmo->root_transform->GetLocalPosition();
    SetGizmoMovePosition((GizmoMoveECS*)gizmo->move_impl, root_pos);
    SetGizmoRotatePosition((GizmoRotateECS*)gizmo->rotate_impl, root_pos);
    SetGizmoScalePosition((GizmoScaleECS*)gizmo->scale_impl, root_pos);
}


GizmoECS *CreateGizmoECS(hgl::ecs::ECSContext *world,
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
            DestroyGizmoECS(gizmo);
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
            DestroyGizmoECS(gizmo);
            return nullptr;
        }

        gizmo->move_impl = (void*)CreateGizmoMoveECS(gizmo->move_world, "GizmoMove", math::Vector3f(0, 0, 0));
        if (!gizmo->move_impl)
        {
            std::cout << "[GizmoECS] Create move gizmo failed" << std::endl;
            DestroyGizmoECS(gizmo);
            return nullptr;
        }
    }

    // Rotate Gizmo
    {
        gizmo->rotate_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Rotate");
        if (!gizmo->rotate_entity)
        {
            std::cout << "[GizmoECS] Create rotate entity failed" << std::endl;
            DestroyGizmoECS(gizmo);
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
            DestroyGizmoECS(gizmo);
            return nullptr;
        }

        gizmo->rotate_impl = (void*)CreateGizmoRotateECS(gizmo->rotate_world, "GizmoRotate", math::Vector3f(0, 0, 0));
        if (!gizmo->rotate_impl)
        {
            std::cout << "[GizmoECS] Create rotate gizmo failed" << std::endl;
            DestroyGizmoECS(gizmo);
            return nullptr;
        }
    }

    // Scale Gizmo
    {
        gizmo->scale_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Scale");
        if (!gizmo->scale_entity)
        {
            std::cout << "[GizmoECS] Create scale entity failed" << std::endl;
            DestroyGizmoECS(gizmo);
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
            DestroyGizmoECS(gizmo);
            return nullptr;
        }

        gizmo->scale_impl = (void*)CreateGizmoScaleECS(gizmo->scale_world, "GizmoScale", math::Vector3f(0, 0, 0));
        if (!gizmo->scale_impl)
        {
            std::cout << "[GizmoECS] Create scale gizmo failed" << std::endl;
            DestroyGizmoECS(gizmo);
            return nullptr;
        }
    }

    // Initialize with Move mode active
    SetGizmoMode(gizmo, GizmoMode::Move);
    SyncAllSubGizmoPositions(gizmo);
    std::cout << "[GizmoECS] Create done" << std::endl;

    return gizmo;
}

void DestroyGizmoECS(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    std::cout << "[GizmoECS] Destroy begin" << std::endl;

    if (gizmo->move_impl)
    {
        DestroyGizmoMoveECS((GizmoMoveECS*)gizmo->move_impl);
    }

    if (gizmo->rotate_impl)
    {
        DestroyGizmoRotateECS((GizmoRotateECS*)gizmo->rotate_impl);
    }

    if (gizmo->scale_impl)
    {
        DestroyGizmoScaleECS((GizmoScaleECS*)gizmo->scale_impl);
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

void SetGizmoMode(GizmoECS *gizmo, GizmoMode mode)
{
    if (!gizmo)
        return;

    gizmo->current_mode = mode;
    std::cout << "[GizmoECS] Set mode=" << static_cast<int>(mode) << std::endl;

    // Set visibility based on mode
    SetGizmoMoveVisible((GizmoMoveECS*)gizmo->move_impl, mode == GizmoMode::Move);
    SetGizmoRotateVisible((GizmoRotateECS*)gizmo->rotate_impl, mode == GizmoMode::Rotate);
    SetGizmoScaleVisible((GizmoScaleECS*)gizmo->scale_impl, mode == GizmoMode::Scale);

    // Pause non-active sub-worlds to avoid concurrent rendering/update artifacts
    if (gizmo->move_subworld)
        gizmo->move_subworld->SetPaused(mode != GizmoMode::Move);
    if (gizmo->rotate_subworld)
        gizmo->rotate_subworld->SetPaused(mode != GizmoMode::Rotate);
    if (gizmo->scale_subworld)
        gizmo->scale_subworld->SetPaused(mode != GizmoMode::Scale);

    gizmo->last_rotate_angle = 0.0f;
    gizmo->last_scale_value = 1.0f;
    gizmo->last_rotate_axis = -1;
    gizmo->last_scale_axis = -1;

    SyncAllSubGizmoPositions(gizmo);
}

GizmoMode GetGizmoMode(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->current_mode : GizmoMode::Move;
}

void SetGizmoVisible(GizmoECS *gizmo, bool visible)
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

hgl::ecs::Entity *GetGizmoRootEntity(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->root : nullptr;
}

void UpdateGizmoECS(GizmoECS *gizmo,
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

    // Update only the active mode
    switch (gizmo->current_mode)
    {
    case GizmoMode::Move:
        UpdateGizmoMoveECS((GizmoMoveECS*)gizmo->move_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            math::Vector3f move_pos;
            if(GetGizmoMovePosition((GizmoMoveECS*)gizmo->move_impl, move_pos))
                gizmo->root_transform->SetLocalPosition(glm::vec3(move_pos));
        }
        break;
    case GizmoMode::Rotate:
    {
        UpdateGizmoRotateECS((GizmoRotateECS*)gizmo->rotate_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            GizmoRotateECSState state;
            if(GetGizmoRotateECSState((GizmoRotateECS*)gizmo->rotate_impl, state))
            {
                if(state.dragging && state.pick_axis >= 0 && state.pick_axis < 3)
                {
                    if(gizmo->last_rotate_axis != state.pick_axis)
                    {
                        gizmo->last_rotate_axis = state.pick_axis;
                        gizmo->last_rotate_angle = state.cur_angle;
                    }

                    const float delta = state.cur_angle - gizmo->last_rotate_angle;
                    if(std::fabs(delta) > 1e-6f)
                    {
                        const math::Vector3f axis = math::GetAxisVector(math::AXIS(state.pick_axis));
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
    case GizmoMode::Scale:
    {
        UpdateGizmoScaleECS((GizmoScaleECS*)gizmo->scale_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            GizmoScaleECSState state;
            if(GetGizmoScaleECSState((GizmoScaleECS*)gizmo->scale_impl, state))
            {
                if(state.dragging && state.pick_axis >= 0 && state.pick_axis < 3)
                {
                    if(gizmo->last_scale_axis != state.pick_axis)
                    {
                        gizmo->last_scale_axis = state.pick_axis;
                        gizmo->last_scale_value = state.cur_scale;
                    }

                    float base = gizmo->last_scale_value;
                    if(std::fabs(base) < 1e-6f)
                        base = 1.0f;

                    const float ratio = state.cur_scale / base;
                    if(std::fabs(ratio - 1.0f) > 1e-6f)
                    {
                        glm::vec3 cur = gizmo->root_transform->GetLocalScale();
                        cur[state.pick_axis] *= ratio;
                        cur = glm::max(cur, glm::vec3(0.05f));
                        gizmo->root_transform->SetLocalScale(cur);
                    }

                    gizmo->last_scale_value = state.cur_scale;
                }
                else
                {
                    gizmo->last_scale_axis = -1;
                    gizmo->last_scale_value = 1.0f;
                }
            }
        }
        break;
    }
    }

    SyncAllSubGizmoPositions(gizmo);
}

}//namespace hgl::graph

