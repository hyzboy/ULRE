#include<hgl/graph/gizmo/TransformGizmoSystem.h>
#include"Gizmo.h"
#include"GizmoInternal.h"
#include"GizmoResource.h"
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/io/event/KeyboardEvent.h>

namespace hgl::graph{

TransformGizmoSystem::TransformGizmoSystem()
    : hgl::ecs::System("TransformGizmoSystem")
    , default_mode(GizmoMode::MoveWorld)
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

void TransformGizmoSystem::SetDefaultMode(GizmoMode mode)
{
    default_mode = mode;
}

GizmoMode TransformGizmoSystem::GetDefaultMode() const
{
    return default_mode;
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

}//namespace hgl::graph
