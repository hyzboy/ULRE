#include<hgl/graph/gizmo/SunDirectionControlSystem.h>
#include"Gizmo.h"
#include"GizmoInternal.h"
#include"GizmoResource.h"
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/LightSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<glm/gtc/quaternion.hpp>
#include<hgl/math/Quaternion.h>

namespace hgl::graph{

SunDirectionControlSystem::SunDirectionControlSystem()
    : hgl::ecs::System("SunDirectionControlSystem")
{
    SetExecutionPhase(hgl::ecs::ExecutionPhase::TickPostCamera);
}

SunDirectionControlSystem::~SunDirectionControlSystem()
{
    Shutdown();
}

bool SunDirectionControlSystem::EnsureLightSystem()
{
    if (!context)
        return false;

    if (light_system)
        return true;

    if (!auto_find_light)
        return false;

    auto sys_sp = context->GetSystem<hgl::ecs::LightSystem>();
    light_system = sys_sp.get();
    return light_system != nullptr;
}

bool SunDirectionControlSystem::EnsureProxyEntity()
{
    if (!context)
        return false;

    if (proxy_entity && proxy_transform.IsValid())
        return true;

    proxy_entity = context->CreateEntity<hgl::ecs::Entity>("SunDirectionGizmoProxy");
    if (!proxy_entity)
        return false;

    context->CreateTransform(proxy_entity->GetEntityID(), hgl::ecs::Mobility::Movable);
    proxy_transform = context->GetTransformByEntity(proxy_entity->GetEntityID());
    if (!proxy_transform.IsValid())
        return false;

    proxy_transform.SetMobility(hgl::ecs::Mobility::Movable);
    proxy_transform.SetLocalTRS(glm::vec3(gizmo_position),
                                 glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                 glm::vec3(1.0f));
    return true;
}

bool SunDirectionControlSystem::EnsureGizmo()
{
    if (gizmo)
        return true;

    if (!context || !proxy_transform.IsValid())
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

    if (!EnsureLightSystem())
        return;

    if (!EnsureProxyEntity())
        return;

    if (!EnsureGizmo())
        return;

    if (light_system)
    {
        if (const auto *main_light = light_system->GetMainLight())
        {
            const math::Vector3f sun_dir(main_light->direction_intensity.x,
                                         main_light->direction_intensity.y,
                                         main_light->direction_intensity.z);

            proxy_transform.SetLocalPosition(glm::vec3(gizmo_position));
            proxy_transform.SetLocalRotation(hgl::math::DirectionToRotation(sun_dir));
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

    proxy_transform = hgl::ecs::TransformAccessor{};
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

    if (!EnsureLightSystem())
        return;

    if (!EnsureProxyEntity())
        return;

    if (!EnsureGizmo())
        return;

    auto input_system = context->GetSystem<hgl::ecs::InputSystem>();
    auto camera_system = context->GetSystem<hgl::ecs::CameraSystem>();
    if (!input_system || !camera_system)
        return;

    const CameraInfo *camera_info = context->GetActiveCameraInfo();
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

    if (light_system)
    {
        if (proxy_transform.IsValid())
            proxy_transform.SetLocalPosition(glm::vec3(gizmo_position));

        math::Vector3f dir(0.0f, 0.0f, 1.0f);
        if (auto *gizmo_root = GetGizmoRootEntity(gizmo))
        {
            auto gizmo_root_transform = context->GetTransformByEntity(gizmo_root->GetEntityID());
            if (gizmo_root_transform.IsValid())
            {
                dir = hgl::math::RotationToDirection(gizmo_root_transform.GetLocalRotation());

                if (proxy_transform.IsValid())
                    proxy_transform.SetLocalRotation(gizmo_root_transform.GetLocalRotation());
            }
        }

        // 写**世界私有光源行 0**（T6b：太阳方向不再寄居 SkyInfo；脏行上传由行表接管，
        // 与旧 `MarkSkyDirty()` 同效但归属正确）
        light_system->SetMainLightDirection(dir);
    }
}

}//namespace hgl::graph
