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
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/SubWorldComponent.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/VisibilityComponent.h>
#include<hgl/ecs/InputSystem.h>
#include<iostream>

USING_COMPONENT_NAMESPACE

VK_NAMESPACE_BEGIN

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
    
    hgl::ecs::ECSContext* move_world = nullptr;
    hgl::ecs::ECSContext* rotate_world = nullptr;
    hgl::ecs::ECSContext* scale_world = nullptr;
    
    GizmoMode current_mode = GizmoMode::Move;
};

// Forward declare the internal gizmo functions
extern GizmoMoveECS *CreateGizmoMoveECS(hgl::ecs::ECSContext *world,
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

extern GizmoRotateECS *CreateGizmoRotateECS(hgl::ecs::ECSContext *world,
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

extern GizmoScaleECS *CreateGizmoScaleECS(hgl::ecs::ECSContext *world,
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

extern void SetGizmoMoveVisible(GizmoMoveECS *gizmo, bool visible);
extern void SetGizmoRotateVisible(GizmoRotateECS *gizmo, bool visible);
extern void SetGizmoScaleVisible(GizmoScaleECS *gizmo, bool visible);


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
        break;
    case GizmoMode::Rotate:
        UpdateGizmoRotateECS((GizmoRotateECS*)gizmo->rotate_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        break;
    case GizmoMode::Scale:
        UpdateGizmoScaleECS((GizmoScaleECS*)gizmo->scale_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        break;
    }
}

VK_NAMESPACE_END
