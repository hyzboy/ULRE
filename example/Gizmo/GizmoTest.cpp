#include<hgl/WorkManager.h>
#include"Gizmo.h"
#include"GizmoResource.h"
#include<hgl/math/VectorTypes.h>
#include<hgl/component/PrimitiveComponent.h>

// ECS headers
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/CameraComponent.h>
#include<hgl/ecs/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<vector>

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

    bool CreateGizmoEntity(const char *name,
                           Primitive *prim,
                           const math::Vector3f &position,
                           const glm::quat &rotation,
                           const math::Vector3f &scale)
    {
        if(!ecs_world || !prim)
            return false;

        auto entity = ecs_world->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(position.x, position.y, position.z));
        transform->SetLocalRotation(rotation);
        transform->SetLocalScale(glm::vec3(scale.x, scale.y, scale.z));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(prim);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitGizmo()
    {
        if(!InitGizmoResource(GetRenderFramework()))
            return(false);

        Primitive *center_sphere = CreateGizmoPrimitive(GizmoShape::Sphere, GizmoColor::White);
        if(!center_sphere)
            return false;

        if(!CreateGizmoEntity("GizmoCenter", center_sphere,
                              math::Vector3f(0.0f, 0.0f, 0.0f),
                              glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                              math::Vector3f(1.0f, 1.0f, 1.0f)))
            return false;

        struct AxisConfig
        {
            math::Vector3f rotation_axis;
            float rotation_angle;
            GizmoColor color;
        };

        const AxisConfig axis_config[3]=
        {
            {math::Vector3f(0.0f, 1.0f, 0.0f),  90.0f, GizmoColor::Red},
            {math::Vector3f(1.0f, 0.0f, 0.0f), -90.0f, GizmoColor::Green},
            {math::Vector3f(0.0f, 0.0f, 0.0f),   0.0f, GizmoColor::Blue}
        };

        const math::Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_HALF_LENGTH);
        const math::Vector3f one_scale(1.0f, 1.0f, 1.0f);

        for(int i=0;i<3;i++)
        {
            const math::Vector3f axis_vector = math::GetAxisVector(math::AXIS(i));
            const AxisConfig &cfg = axis_config[i];

            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if(cfg.rotation_angle != 0.0f)
            {
                rotation = glm::angleAxis(glm::radians(cfg.rotation_angle),
                                          glm::vec3(cfg.rotation_axis.x, cfg.rotation_axis.y, cfg.rotation_axis.z));
            }

            Primitive *cylinder = CreateGizmoPrimitive(GizmoShape::Cylinder, cfg.color);
            if(!cylinder)
                return false;

            const math::Vector3f cylinder_pos = axis_vector * GIZMO_CYLINDER_OFFSET;
            if(!CreateGizmoEntity((i==0) ? "GizmoX_Cylinder" : (i==1) ? "GizmoY_Cylinder" : "GizmoZ_Cylinder",
                                  cylinder, cylinder_pos, rotation, cylinder_scale))
                return false;

            Primitive *cone = CreateGizmoPrimitive(GizmoShape::Cone, cfg.color);
            if(!cone)
                return false;

            const math::Vector3f cone_pos = axis_vector * GIZMO_CONE_OFFSET;
            if(!CreateGizmoEntity((i==0) ? "GizmoX_Cone" : (i==1) ? "GizmoY_Cone" : "GizmoZ_Cone",
                                  cone, cone_pos, rotation, one_scale))
                return false;
        }

        return true;
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
        for(Primitive *prim : gizmo_primitives)
            delete prim;
        gizmo_primitives.clear();

        FreeGizmoResource();
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
