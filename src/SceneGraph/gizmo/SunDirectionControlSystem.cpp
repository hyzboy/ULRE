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
#include<hgl/graph/camera/ViewportInfo.h>
#include<glm/gtc/quaternion.hpp>
#include<hgl/math/Quaternion.h>
#include<hgl/log/Log.h>
#include<cstdio>

namespace hgl::graph{
namespace
{
    constexpr uint32_t kMaxGizmoResourceInitAttempts = 120;
}

SunDirectionControlSystem::SunDirectionControlSystem()
    : hgl::ecs::System("SunDirectionControlSystem")
{
    SetExecutionOrder(hgl::ecs::ExecutionPhase::TickPostCamera);
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
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] EnsureProxyEntity failed: context=null\n");
        return false;
    }

    if (proxy_entity && proxy_transform)
        return true;

    proxy_entity = context->CreateEntity<hgl::ecs::Entity>("SunDirectionGizmoProxy");
    if (!proxy_entity)
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] EnsureProxyEntity failed: CreateEntity returned null\n");
        return false;
    }

    proxy_transform = proxy_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
    if (!proxy_transform)
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] EnsureProxyEntity failed: AddComponent<TransformComponent> returned null\n");
        return false;
    }

    proxy_transform->SetMovable(true);
    proxy_transform->SetLocalTRS(glm::vec3(gizmo_position),
                                 glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                 glm::vec3(1.0f));

    std::fprintf(stderr,
                 "[SunDirectionControlSystem] EnsureProxyEntity ok: proxy_entity=%p proxy_transform=%p pos=(%.3f,%.3f,%.3f)\n",
                 static_cast<void *>(proxy_entity),
                 static_cast<void *>(proxy_transform.get()),
                 gizmo_position.x, gizmo_position.y, gizmo_position.z);
    return true;
}

bool SunDirectionControlSystem::EnsureGizmo()
{
    if (gizmo)
        return true;

    if (!context || !proxy_transform)
    {
        std::fprintf(stderr,
                     "[SunDirectionControlSystem] EnsureGizmo failed: context=%p proxy_transform=%p\n",
                     static_cast<void *>(context),
                     static_cast<void *>(proxy_transform.get()));
        return false;
    }

    gizmo = CreateDefaultTransformGizmo(context, "SunDirectionGizmo", gizmo_position, GizmoMode::RotateWorld);
    if (!gizmo)
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] EnsureGizmo failed: CreateDefaultTransformGizmo returned null\n");
        return false;
    }

    BindTransformGizmoTargetEntity(gizmo, proxy_entity);
    SetTransformGizmoMode(gizmo, GizmoMode::RotateWorld);
    SetTransformGizmoAllowNegativeScale(gizmo, false);

    std::fprintf(stderr,
                 "[SunDirectionControlSystem] EnsureGizmo ok: gizmo=%p target_proxy=%p mode=%d\n",
                 static_cast<void *>(gizmo),
                 static_cast<void *>(proxy_entity),
                 static_cast<int>(GetTransformGizmoMode(gizmo)));
    return true;
}

bool SunDirectionControlSystem::EnsureGizmoResourcesWithRetry(const char *stage_tag)
{
    if (IsGizmoSystemResourcesResident())
        return true;

    if (gizmo_resource_init_permanent_failure)
        return false;

    ++gizmo_resource_init_attempts;

    if (EnsureGizmoSystemResources(context))
    {
        LogInfo("[SunDirectionControlSystem] %s: EnsureGizmoSystemResources success on attempt=%u",
                stage_tag ? stage_tag : "Unknown",
                gizmo_resource_init_attempts);
        return true;
    }

    LogWarning("[SunDirectionControlSystem] %s: EnsureGizmoSystemResources failed attempt=%u/%u",
               stage_tag ? stage_tag : "Unknown",
               gizmo_resource_init_attempts,
               kMaxGizmoResourceInitAttempts);

    if (gizmo_resource_init_attempts >= kMaxGizmoResourceInitAttempts)
    {
        gizmo_resource_init_permanent_failure = true;
        LogWarning("[SunDirectionControlSystem] %s: resource init reached retry limit=%u; mark permanent failure",
                   stage_tag ? stage_tag : "Unknown",
                   kMaxGizmoResourceInitAttempts);
    }

    return false;
}

void SunDirectionControlSystem::Initialize()
{
    std::fprintf(stderr,
                 "[SunDirectionControlSystem] Initialize begin: context=%p env=%p auto_find=%d\n",
                 static_cast<void *>(context),
                 static_cast<void *>(environment_system),
                 auto_find_environment ? 1 : 0);
    LogInfo("[SunDirectionControlSystem] Initialize begin: context=%p env=%p auto_find=%d",
            static_cast<void *>(context),
            static_cast<void *>(environment_system),
            auto_find_environment ? 1 : 0);

    if (!resource_registered)
    {
        ++g_gizmo_resident_state.active_system_count;
        resource_registered = true;
    }

    if (!EnsureGizmoResourcesWithRetry("Initialize"))
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Initialize abort: EnsureGizmoSystemResources failed\n");
        LogWarning("[SunDirectionControlSystem] Initialize abort: EnsureGizmoSystemResources failed");
        return;
    }

    if (!EnsureEnvironment())
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Initialize abort: EnsureEnvironment failed\n");
        LogWarning("[SunDirectionControlSystem] Initialize abort: EnsureEnvironment failed");
        return;
    }

    if (!EnsureProxyEntity())
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Initialize abort: EnsureProxyEntity failed\n");
        LogWarning("[SunDirectionControlSystem] Initialize abort: EnsureProxyEntity failed");
        return;
    }

    if (!EnsureGizmo())
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Initialize abort: EnsureGizmo failed\n");
        LogWarning("[SunDirectionControlSystem] Initialize abort: EnsureGizmo failed");
        return;
    }

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

    std::fprintf(stderr,
                 "[SunDirectionControlSystem] Initialize done: gizmo=%p proxy=%p env=%p\n",
                 static_cast<void *>(gizmo),
                 static_cast<void *>(proxy_entity),
                 static_cast<void *>(environment_system));
    LogInfo("[SunDirectionControlSystem] Initialize done: gizmo=%p proxy=%p env=%p",
            static_cast<void *>(gizmo),
            static_cast<void *>(proxy_entity),
            static_cast<void *>(environment_system));
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
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Update skip: context=null\n");
        return;
    }

    if (gizmo_resource_init_permanent_failure)
        return;

    if (!IsGizmoSystemResourcesResident())
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Update: gizmo resources not resident, rebuilding\n");

        if (gizmo)
        {
            DestroyTransformGizmo(gizmo);
            gizmo = nullptr;
        }

        if (!EnsureGizmoResourcesWithRetry("Update"))
        {
            std::fprintf(stderr, "[SunDirectionControlSystem] Update abort: EnsureGizmoSystemResources failed\n");
            return;
        }
    }

    if (!EnsureEnvironment())
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Update abort: EnsureEnvironment failed\n");
        return;
    }

    if (!EnsureProxyEntity())
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Update abort: EnsureProxyEntity failed\n");
        return;
    }

    if (!EnsureGizmo())
    {
        std::fprintf(stderr, "[SunDirectionControlSystem] Update abort: EnsureGizmo failed\n");
        return;
    }

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
