#include<hgl/WorkManager.h>
#include"Gizmo.h"
#include"GizmoResource.h"
#include<hgl/math/VectorTypes.h>
#include<hgl/graph/font/TextRender.h>
#include<hgl/graph/font/TextGeometry.h>
#include<hgl/utf.h>

// ECS headers
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/CameraComponent.h>
#include<hgl/ecs/CameraSystem.h>
#include<hgl/ecs/InputSystem.h>
#include<hgl/io/event/KeyboardEvent.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<vector>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;

const math::Vector3f GizmoPosition(0,0,0);


///**
//* 一种永远转向正面的变换节点
//*/
//class TransformBillboard:public TransformAction
//{
//    CameraInfo *camera_info=nullptr;
//    bool face_to_camera=false;
//
//    ViewportInfo *viewport_info=nullptr;
//    float fixed_scale=1.0;
//
//public:
//
//    virtual void SetCameraInfo  (CameraInfo *   ci  ){camera_info   =ci;}
//    virtual void SetViewportInfo(ViewportInfo * vi  ){viewport_info =vi;}
//
//    virtual void SetFaceToCamera(bool           ftc ){face_to_camera=ftc;}
//    virtual void SetFixedScale  (const float    size){fixed_scale   =size;}
//
//    virtual bool RefreshTransform(const Transform &tf=IdentityTransform) override
//    {
//        if(!camera_info)
//        {
//            return SceneNode::RefreshTransform(tf);
//        }
//
//        if(face_to_camera)
//        {
//            LocalTransform.SetRotation(CalculateFacingRotationQuat(GetWorldPosition(),camera_info->view,AxisVector::X));
//        }
//
//        if(viewport_info)
//        {
//            const float screen_height=viewport_info->GetViewportHeight();
//
//            const math::Vector4f pos=camera_info->Project(GetWorldPosition());
//
//            LocalTransform.SetScale(pos.w*fixed_scale/screen_height);
//        }
//
//        return SceneNode::RefreshTransform(tf);
//    }
//};//class BillboardSceneNode:public SceneNode

class TestApp:public WorkObject
{
    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    Pipeline *gizmo_pipeline = nullptr;
    std::vector<Primitive *> gizmo_primitives;
    GizmoECS *gizmo = nullptr;  // 统一的 Gizmo 世界
    
    bool last_left_down = false;
    bool last_key_w = false;
    bool last_key_e = false;
    bool last_key_r = false;

    TextRender *debug_text_render = nullptr;
    TextGeometry *debug_text_geom = nullptr;
    Primitive *debug_text_prim = nullptr;
    hgl::ecs::Entity *debug_text_entity = nullptr;
    std::shared_ptr<hgl::ecs::PrimitiveComponent> debug_text_comp;
    std::string debug_text_cache;

private:

    Primitive *CreateGizmoPrimitive(const GizmoShape &shape,const GizmoColor &color)
    {
        Primitive *base_prim = GetGizmoMeshPrimitive(shape);
        if(!base_prim)
            return nullptr;

        Geometry *geometry = base_prim->GetGeometry();
        MaterialInstance *mi = GetGizmoMI3D(color);
        if(!geometry || !mi)
            return nullptr;

        if(!gizmo_pipeline)
            gizmo_pipeline = CreatePipeline(mi, InlinePipeline::Solid3D);
        if(!gizmo_pipeline)
            return nullptr;

        Primitive *prim = CreatePrimitive(geometry, mi, gizmo_pipeline);
        if(prim)
            gizmo_primitives.push_back(prim);

        return prim;
    }

    bool InitGizmo()
    {
        if(!InitGizmoResource(GetRenderFramework()))
            return(false);

        if(!ecs_world)
            return false;

        gizmo = CreateGizmoECS(ecs_world, "Gizmo", GizmoPosition);
        if(!gizmo)
            return false;

        return true;
    }

    bool InitDebugOverlay()
    {
        if(!ecs_world)
            return false;

        FontSource *fs = CreateFontSource(OS_TEXT("Consolas"), 16);
        if(!fs)
            return false;

        debug_text_render = CreateTextRender(fs, 256);
        if(!debug_text_render)
            return false;

        U16String initial_text = U16_TEXT("gizmo debug");
        debug_text_geom = debug_text_render->CreateGeometry(TextGeometryType::FixedStyle, initial_text);
        if(!debug_text_geom || !debug_text_geom->IsValid())
            return false;

        debug_text_prim = debug_text_render->CreatePrimitive(debug_text_geom);
        if(!debug_text_prim)
            return false;

        debug_text_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("GizmoDebugText");
        if(!debug_text_entity)
            return false;

        auto transform = debug_text_entity->AddComponent<hgl::ecs::TransformComponent>();
        transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

        debug_text_comp = debug_text_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        if(!debug_text_comp)
            return false;

        debug_text_comp->SetPrimitive(debug_text_prim);
        return true;
    }

    void UpdateDebugOverlay(hgl::ecs::InputSystem *input_system)
    {
        if(!debug_text_render || !debug_text_geom || !input_system)
            return;

        GizmoMode mode = GetGizmoMode(gizmo);
        std::string text = "mode=";
        text += std::to_string(static_cast<int>(mode));
        text += " (1=Move, 2=Rotate, 3=Scale)";
        text += " capture=";
        text += input_system->IsMouseCaptured() ? "1" : "0";
        text += " left=";
        text += input_system->IsMouseButtonDown(hgl::io::MouseButton::Left) ? "1" : "0";

        if(text == debug_text_cache)
            return;

        debug_text_cache = text;

        //U16String u16_text = to_u16(reinterpret_cast<const u8char *>(text.c_str()));
        //debug_text_render->SimpleLayout(debug_text_geom, u16_text);
        std::cout<<text<<std::endl;
    }

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

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        GetSceneRenderer()->SetCameraControl(nullptr);

        if(!InitGizmo())
            return false;

        if(!InitCamera())
            return false;

        if(!InitDebugOverlay())
            return false;

        return true;
    }

public:

    bool Init() override
    {
        if(!InitECS())
            return(false);

        return(true);
    }

    using WorkObject::WorkObject;

    ~TestApp()
    {
        if(ecs_world && debug_text_entity)
        {
            ecs_world->DestroyEntity(debug_text_entity->GetID());
            debug_text_entity = nullptr;
        }

        SAFE_CLEAR(debug_text_render);
        debug_text_geom = nullptr;
        debug_text_prim = nullptr;
        debug_text_comp.reset();

        if(gizmo)
        {
            DestroyGizmoECS(gizmo);
            gizmo = nullptr;
        }

        for(Primitive *prim : gizmo_primitives)
            delete prim;
        gizmo_primitives.clear();

        FreeGizmoResource();
    }

    void Tick(double delta) override
    {
        if(ecs_world)
        {
            auto input_system = ecs_world->GetSystem<hgl::ecs::InputSystem>();
            if(input_system)
            {
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
                    std::cout << ">>> Switched to Move mode <<<" << std::endl;
                }
                else if(key_2 && !last_key_e)
                {
                    SetGizmoMode(gizmo, GizmoMode::Rotate);
                    std::cout << ">>> Switched to Rotate mode <<<" << std::endl;
                }
                else if(key_3 && !last_key_r)
                {
                    SetGizmoMode(gizmo, GizmoMode::Scale);
                    std::cout << ">>> Switched to Scale mode <<<" << std::endl;
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

                UpdateDebugOverlay(input_system.get());
            }
            else
            {
                static bool system_error_printed = false;
                if(!system_error_printed)
                {
                    std::cout << "ERROR: InputSystem not found in ECSContext!" << std::endl;
                    system_error_printed = true;
                }
            }
        }

        WorkObject::Tick(delta);
    }

    //void BuildCommandBuffer(uint32 index) override
    //{
    //    camera_control->Refresh();
    //
    //    const CameraInfo *ci=camera_control->GetCameraInfo();
    //    const ViewportInfo *vi=GetViewportInfo();

    //    const float screen_height=vi->GetViewportHeight();

    //    const math::Vector4f pos=ci->Project(GizmoPosition);

    //    //{
    //    //    Transform tm;

    //    //    tm.SetScale(pos.w*16.0f/screen_height);

    //    //    root.SetLocalTransform(tm);
    //    //}
    //}
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Gizmo"),1280,720);
}
