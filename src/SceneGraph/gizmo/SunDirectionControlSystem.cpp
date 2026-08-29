#include<hgl/graph/gizmo/SunDirectionControlSystem.h>
#include"Gizmo.h"
#include"GizmoInternal.h"
#include"GizmoResource.h"
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<glm/gtc/quaternion.hpp>
#include<hgl/math/Quaternion.h>

namespace hgl::graph{

SunDirectionControlSystem::SunDirectionControlSystem()
    : hgl::ecs::System("SunDirectionControlSystem")
{
    SetExecutionPhase(hgl::ecs::ExecutionPhase::TickPostCamera);
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

    proxy_transform = proxy_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
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
            proxy_transform->SetLocalRotation(hgl::math::DirectionToRotation(sun_dir));
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
        context->DestroyEntity(proxy_entity->GetEntityID());
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

    GizmoFrameInput frame_input;
    frame_input.mouse_coord   = mouse_coord;
    frame_input.camera_info   = camera_info;
    frame_input.viewport_info = viewport_info;
    frame_input.input_system  = input_system.get();
    frame_input.left_down     = left_down;
    frame_input.left_pressed  = left_pressed;
    frame_input.left_released = left_released;
    UpdateTransformGizmo(gizmo, frame_input);

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
                    dir = hgl::math::RotationToDirection(gizmo_root_transform->GetLocalRotation());

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
