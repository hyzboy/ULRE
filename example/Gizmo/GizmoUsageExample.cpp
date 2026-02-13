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
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/CameraComponent.h>
#include<hgl/ecs/CameraSystem.h>
#include<hgl/ecs/InputSystem.h>
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
    
    GizmoMoveECS *gizmo_move = nullptr;
    GizmoRotateECS *gizmo_rotate = nullptr;
    GizmoScaleECS *gizmo_scale = nullptr;
    
    enum class GizmoMode
    {
        Move,
        Rotate,
        Scale
    };
    
    GizmoMode current_mode = GizmoMode::Move;
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
        if(!InitGizmoResource(GetRenderFramework()))
            return false;

        if(!ecs_world)
            return false;
        
        // 创建 Move Gizmo
        gizmo_move = CreateGizmoMoveECS(ecs_world, "GizmoMove", GizmoPosition);
        if(!gizmo_move)
            return false;
        
        // 创建 Rotate Gizmo
        gizmo_rotate = CreateGizmoRotateECS(ecs_world, "GizmoRotate", GizmoPosition);
        if(!gizmo_rotate)
            return false;
        
        // 创建 Scale Gizmo
        gizmo_scale = CreateGizmoScaleECS(ecs_world, "GizmoScale", GizmoPosition);
        if(!gizmo_scale)
            return false;
        
        return true;
    }

    void UpdateDebug(hgl::ecs::InputSystem *input_system)
    {
        if(!input_system)
            return;

        std::string text = "mode=";
        if(current_mode == GizmoMode::Move)
            text += "Move(1)";
        else if(current_mode == GizmoMode::Rotate)
            text += "Rotate(2)";
        else if(current_mode == GizmoMode::Scale)
            text += "Scale(3)";
        
        text += " left=";
        text += input_system->IsMouseButtonDown(hgl::io::MouseButton::Left) ? "1" : "0";

        if(current_mode == GizmoMode::Move && gizmo_move)
        {
            GizmoMoveECSState state;
            if(GetGizmoMoveECSState(gizmo_move, state))
            {
                text += " dragging=";
                text += state.dragging ? "1" : "0";
                text += " axis=";
                text += std::to_string(state.cur_axis);
            }
        }
        else if(current_mode == GizmoMode::Rotate && gizmo_rotate)
        {
            GizmoRotateECSState state;
            if(GetGizmoRotateECSState(gizmo_rotate, state))
            {
                text += " dragging=";
                text += state.dragging ? "1" : "0";
                text += " axis=";
                text += std::to_string(state.cur_axis);
            }
        }
        else if(current_mode == GizmoMode::Scale && gizmo_scale)
        {
            GizmoScaleECSState state;
            if(GetGizmoScaleECSState(gizmo_scale, state))
            {
                text += " dragging=";
                text += state.dragging ? "1" : "0";
                text += " axis=";
                text += std::to_string(state.cur_axis);
            }
        }

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

        GetSceneRenderer()->SetCameraControl(nullptr);

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
        if(gizmo_move)
        {
            DestroyGizmoMoveECS(gizmo_move);
            gizmo_move = nullptr;
        }
        
        if(gizmo_rotate)
        {
            DestroyGizmoRotateECS(gizmo_rotate);
            gizmo_rotate = nullptr;
        }
        
        if(gizmo_scale)
        {
            DestroyGizmoScaleECS(gizmo_scale);
            gizmo_scale = nullptr;
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
            current_mode = GizmoMode::Move;
            std::cout << "Switched to Move mode" << std::endl;
        }
        else if(key_2 && !last_key_e)
        {
            current_mode = GizmoMode::Rotate;
            std::cout << "Switched to Rotate mode" << std::endl;
        }
        else if(key_3 && !last_key_r)
        {
            current_mode = GizmoMode::Scale;
            std::cout << "Switched to Scale mode" << std::endl;
        }
        
        last_key_w = key_1;
        last_key_e = key_2;
        last_key_r = key_3;
        
        // 更新当前激活的 Gizmo
        if(current_mode == GizmoMode::Move && gizmo_move)
        {
            UpdateGizmoMoveECS(gizmo_move,
                               mouse_coord,
                               GetCameraInfo(),
                               GetViewportInfo(),
                               input_system.get(),
                               left_down,
                               left_pressed,
                               left_released);
        }
        else if(current_mode == GizmoMode::Rotate && gizmo_rotate)
        {
            UpdateGizmoRotateECS(gizmo_rotate,
                                 mouse_coord,
                                 GetCameraInfo(),
                                 GetViewportInfo(),
                                 input_system.get(),
                                 left_down,
                                 left_pressed,
                                 left_released);
            // 非激活的 Gizmo 只会更新内部状态（无交互）
            if(gizmo_move)
            {
                UpdateGizmoMoveECS(gizmo_move,
                                   mouse_coord,
                                   GetCameraInfo(),
                                   GetViewportInfo(),
                                   nullptr,  // 不传入 input_system，禁用交互
                                   false,
                                   false,
                                   false);
            }
            if(gizmo_scale)
            {
                UpdateGizmoScaleECS(gizmo_scale,
                                    mouse_coord,
                                    GetCameraInfo(),
                                    GetViewportInfo(),
                                    nullptr,  // 不传入 input_system，禁用交互
                                    false,
                                    false,
                                    false);
            }
        }
        else if(current_mode == GizmoMode::Scale && gizmo_scale)
        {
            UpdateGizmoScaleECS(gizmo_scale,
                                mouse_coord,
                                GetCameraInfo(),
                                GetViewportInfo(),
                                input_system.get(),
                                left_down,
                                left_pressed,
                                left_released);
            // 非激活的 Gizmo 只会更新内部状态（无交互）
            if(gizmo_move)
            {
                UpdateGizmoMoveECS(gizmo_move,
                                   mouse_coord,
                                   GetCameraInfo(),
                                   GetViewportInfo(),
                                   nullptr,  // 不传入 input_system，禁用交互
                                   false,
                                   false,
                                   false);
            }
            if(gizmo_rotate)
            {
                UpdateGizmoRotateECS(gizmo_rotate,
                                     mouse_coord,
                                     GetCameraInfo(),
                                     GetViewportInfo(),
                                     nullptr,  // 不传入 input_system，禁用交互
                                     false,
                                     false,
                                     false);
            }
        }

        UpdateDebug(input_system.get());
        WorkObject::Tick(delta);
    }
};

int os_main(int, os_char **)
{
    return RunFramework<GizmoExampleApp>(OS_TEXT("Gizmo Usage Example"), 1280, 720);
}
