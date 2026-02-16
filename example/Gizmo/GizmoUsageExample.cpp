/*
 Gizmo ECS 使用示例

 展示如何同时使用 Move, Rotate, Scale 三种 Gizmo
 按键 W/E/R 切换模式
*/

#include<hgl/WorkManager.h>
#include"Gizmo.h"
#include"GizmoResource.h"
#include<hgl/math/VectorTypes.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/io/event/KeyboardEvent.h>

#include<glm/glm.hpp>
#include<iostream>
#include<string>

using namespace hgl;
using namespace hgl::graph;

const math::Vector3f GizmoPosition(0, 0, 0);

class GizmoExampleApp : public WorkObject
{
private:
    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    GizmoECS *gizmo = nullptr;
    bool last_left_down = false;
    bool last_key_w = false;
    bool last_key_e = false;
    bool last_key_r = false;
    std::string debug_cache;

    bool EnsureCameraSystem()
    {
        if(!ecs_world)
            return false;

        auto camera_system = ecs_world->GetSystem<hgl::ecs::CameraSystem>();
        if(!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<hgl::ecs::CameraSystem>(ecs_world);
            if(ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    bool InitCamera()
    {
        if(!EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 48.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<hgl::graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitGizmos()
    {
        auto *ecs = GetECSContext();
        if(!ecs)
            return false;

        auto *graphics = ecs->GetGraphicsContext();
        if(!graphics)
            return false;

        auto *render_context = GetRenderContext();
        auto *render_target = render_context ? render_context->GetCurrentRenderTarget() : nullptr;
        auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        if(!render_pass)
            return false;

        if(!InitGizmoResource(graphics, render_pass))
            return false;

        if(!ecs_world)
            return false;

        gizmo = CreateGizmoECS(ecs_world, "Gizmo", GizmoPosition);
        if(!gizmo)
            return false;

        return true;
    }

    void UpdateDebug(hgl::ecs::InputSystem *input_system)
    {
        if(!input_system || !gizmo)
            return;

        GizmoMode mode = GetGizmoMode(gizmo);
        std::string text = "mode=";
        if(mode == GizmoMode::Move)
            text += "Move(1)";
        else if(mode == GizmoMode::Rotate)
            text += "Rotate(2)";
        else if(mode == GizmoMode::Scale)
            text += "Scale(3)";

        text += " left=";
        text += input_system->IsMouseButtonDown(hgl::io::MouseButton::Left) ? "1" : "0";

        if(text != debug_cache)
        {
            debug_cache = text;
            std::cout << text << std::endl;
        }
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;


        if(!InitGizmos())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:

    bool Init() override
    {
        if(!InitECS())
            return false;

        std::cout << "Gizmo Example Started." << std::endl;
        std::cout << "Press W for Move, E for Rotate, R for Scale" << std::endl;

        return true;
    }

    using WorkObject::WorkObject;

    ~GizmoExampleApp()
    {
        if(gizmo)
        {
            DestroyGizmoECS(gizmo);
            gizmo = nullptr;
        }

        FreeGizmoResource();
    }

    void Tick(double delta) override
    {
        if(!ecs_world)
            return;

        auto input_system = ecs_world->GetSystem<hgl::ecs::InputSystem>();
        if(!input_system)
            return;

        const math::Vector2i &mouse_coord = input_system->GetMouseCoord();
        const bool left_down = input_system->IsMouseButtonDown(hgl::io::MouseButton::Left);
        const bool left_pressed = left_down && !last_left_down;
        const bool left_released = !left_down && last_left_down;
        last_left_down = left_down;

        // 切换 Gizmo 模式（按键 1/2/3）
        const bool key_1 = input_system->IsKeyDown(hgl::io::KeyboardButton::_1);
        const bool key_2 = input_system->IsKeyDown(hgl::io::KeyboardButton::_2);
        const bool key_3 = input_system->IsKeyDown(hgl::io::KeyboardButton::_3);

        if(key_1 && !last_key_w)
        {
            SetGizmoMode(gizmo, GizmoMode::Move);
            std::cout << "Switched to Move mode" << std::endl;
        }
        else if(key_2 && !last_key_e)
        {
            SetGizmoMode(gizmo, GizmoMode::Rotate);
            std::cout << "Switched to Rotate mode" << std::endl;
        }
        else if(key_3 && !last_key_r)
        {
            SetGizmoMode(gizmo, GizmoMode::Scale);
            std::cout << "Switched to Scale mode" << std::endl;
        }

        last_key_w = key_1;
        last_key_e = key_2;
        last_key_r = key_3;

        // 更新统一的 Gizmo（内部会根据当前模式处理）
        UpdateGizmoECS(gizmo,
                       mouse_coord,
                       GetCameraInfo(),
                       GetViewportInfo(),
                       input_system.get(),
                       left_down,
                       left_pressed,
                       left_released);

        UpdateDebug(input_system.get());
        WorkObject::Tick(delta);
    }
};

int os_main(int, os_char **)
{
    return RunFramework<GizmoExampleApp>(OS_TEXT("Gizmo Usage Example"), 1280, 720);
}

