#include<hgl/WorkManager.h>
#include"Gizmo.h"
#include"GizmoResource.h"
#include<hgl/math/VectorTypes.h>
#include<hgl/component/PrimitiveComponent.h>
#include<hgl/component/CreateComponentInfo.h>
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
    GizmoMoveECS *gizmo_move = nullptr;
    GizmoRotateECS *gizmo_rotate = nullptr;
    GizmoScaleECS *gizmo_scale = nullptr;
    
    enum class GizmoMode
    {
        Move,
        Rotate,
        Scale
    };
    GizmoMode current_gizmo_mode = GizmoMode::Move;
    
    bool last_left_down = false;
    bool last_key_w = false;
    bool last_key_e = false;
    bool last_key_r = false;

    TextRender *debug_text_render = nullptr;
    TextGeometry *debug_text_geom = nullptr;
    Primitive *debug_text_prim = nullptr;
    PrimitiveComponent *debug_text_comp = nullptr;
    std::string debug_text_cache;

private:

    Primitive *CreateGizmoPrimitive(const GizmoShape &shape,const GizmoColor &color)
    {
        COMPONENT_NAMESPACE::ComponentDataPtr cdp = GetGizmoMeshCDP(shape);
        auto *mcd = dynamic_cast<COMPONENT_NAMESPACE::PrimitiveComponentData *>(cdp.get());
        if(!mcd || !mcd->primitive)
            return nullptr;

        Geometry *geometry = mcd->primitive->GetGeometry();
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

        gizmo_move = CreateGizmoMoveECS(ecs_world, "GizmoMove", GizmoPosition);
        if(!gizmo_move)
            return false;

        gizmo_rotate = CreateGizmoRotateECS(ecs_world, "GizmoRotate", GizmoPosition);
        if(!gizmo_rotate)
            return false;

        gizmo_scale = CreateGizmoScaleECS(ecs_world, "GizmoScale", GizmoPosition);
        if(!gizmo_scale)
            return false;

        return true;
    }

    bool InitDebugOverlay()
    {
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

        CreateComponentInfo cci(GetWorldRootNode());
        debug_text_comp = CreateComponent<PrimitiveComponent>(&cci, debug_text_prim);
        return debug_text_comp != nullptr;
    }

    void UpdateDebugOverlay(hgl::ecs::InputSystem *input_system)
    {
        if(!debug_text_render || !debug_text_geom || !input_system)
            return;

        std::string text = "mode=";
        if(current_gizmo_mode == GizmoMode::Move)
            text += "Move(W)";
        else if(current_gizmo_mode == GizmoMode::Rotate)
            text += "Rotate(E)";
        else if(current_gizmo_mode == GizmoMode::Scale)
            text += "Scale(R)";
        
        text += " capture=";
        text += input_system->IsMouseCaptured() ? "1" : "0";
        text += " left=";
        text += input_system->IsMouseButtonDown(0) ? "1" : "0";

        if(current_gizmo_mode == GizmoMode::Move && gizmo_move)
        {
            GizmoMoveECSState state;
            if(GetGizmoMoveECSState(gizmo_move, state))
            {
                text += " dragging=";
                text += state.dragging ? "1" : "0";
                text += " axis=";
                text += std::to_string(state.cur_axis);
                text += " dist=";
                text += std::to_string(state.cur_dist);
            }
        }
        else if(current_gizmo_mode == GizmoMode::Rotate && gizmo_rotate)
        {
            GizmoRotateECSState state;
            if(GetGizmoRotateECSState(gizmo_rotate, state))
            {
                text += " dragging=";
                text += state.dragging ? "1" : "0";
                text += " axis=";
                text += std::to_string(state.cur_axis);
                text += " angle=";
                text += std::to_string(state.cur_angle);
            }
        }
        else if(current_gizmo_mode == GizmoMode::Scale && gizmo_scale)
        {
            GizmoScaleECSState state;
            if(GetGizmoScaleECSState(gizmo_scale, state))
            {
                text += " dragging=";
                text += state.dragging ? "1" : "0";
                text += " axis=";
                text += std::to_string(state.cur_axis);
                text += " scale=";
                text += std::to_string(state.cur_scale);
            }
        }

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
        SAFE_CLEAR(debug_text_render);
        debug_text_geom = nullptr;
        debug_text_prim = nullptr;
        debug_text_comp = nullptr;

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
                const bool left_down = input_system->IsMouseButtonDown(0);
                const bool left_pressed = left_down && !last_left_down;
                const bool left_released = !left_down && last_left_down;
                last_left_down = left_down;

                // 切换 Gizmo 模式（按键 W/E/R）
                const bool key_w = input_system->IsKeyDown('W');
                const bool key_e = input_system->IsKeyDown('E');
                const bool key_r = input_system->IsKeyDown('R');
                
                if(key_w && !last_key_w)
                    current_gizmo_mode = GizmoMode::Move;
                else if(key_e && !last_key_e)
                    current_gizmo_mode = GizmoMode::Rotate;
                else if(key_r && !last_key_r)
                    current_gizmo_mode = GizmoMode::Scale;
                
                last_key_w = key_w;
                last_key_e = key_e;
                last_key_r = key_r;

                // 更新当前激活的 Gizmo
                if(current_gizmo_mode == GizmoMode::Move && gizmo_move)
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
                else if(current_gizmo_mode == GizmoMode::Rotate && gizmo_rotate)
                {
                    UpdateGizmoRotateECS(gizmo_rotate,
                                         mouse_coord,
                                         GetCameraInfo(),
                                         GetViewportInfo(),
                                         input_system.get(),
                                         left_down,
                                         left_pressed,
                                         left_released);
                }
                else if(current_gizmo_mode == GizmoMode::Scale && gizmo_scale)
                {
                    UpdateGizmoScaleECS(gizmo_scale,
                                        mouse_coord,
                                        GetCameraInfo(),
                                        GetViewportInfo(),
                                        input_system.get(),
                                        left_down,
                                        left_pressed,
                                        left_released);
                }

                UpdateDebugOverlay(input_system.get());
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
