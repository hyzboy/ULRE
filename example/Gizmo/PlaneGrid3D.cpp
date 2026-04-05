// PlaneGrid3D

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    Material *          material            =nullptr;

    Geometry *         geom_plane_grid     =nullptr;
    MaterialInstance *  material_instance[3]{};

private:

    bool InitMDP()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        static const mtl::MaterialAssetRecord kPlaneGridCfg {
            .id       = "plane_grid",
            .preset   = mtl::MaterialPreset::VertexLuminance2D,
            .prim     = PrimitiveType::Lines,
            .pipeline = GraphicsPipelinePreset::Solid3D,
            .mi_vil_overrides = {
                { VAN::Luminance, VF_V1UN8 },
            },
        };

        MaterialAssetRegistry registry(material_manager, nullptr, nullptr);
        Color4f GridColor;
        COLOR ce=COLOR::BlenderAxisRed;

        for(uint i=0;i<3;i++)
        {
            GridColor=GetColor4f(ce,1.0);

            material_instance[i] = registry.AcquireMI(kPlaneGridCfg, &GridColor, sizeof(GridColor));

            if(i == 0 && material_instance[i])
                material = material_instance[i]->GetMaterial();

            ce=COLOR((int)ce+1);
        }

        return material_instance[0] && material_instance[1] && material_instance[2];
    }

    bool CreateRenderObject()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        struct PlaneGridCreateInfo pgci;

        pgci.grid_size.Set(32,32);
        pgci.sub_count.Set(8,8);

        pgci.lum=180;
        pgci.sub_lum=255;

        auto pc = std::make_unique<GeometryCreater>(device, material_instance[0]->GetVIL());

        geom_plane_grid=CreatePlaneGrid2D(pc.get(),&pgci);
        if (geom_plane_grid)
            geometry_manager->Add(geom_plane_grid);

        return geom_plane_grid;
    }

    bool Add(const char *name,MaterialInstance *mi,const glm::quat &rotation)
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        Primitive *ri=primitive_manager->CreatePrimitive(geom_plane_grid,mi);

        if(!ri)
            return false;

        auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(rotation);
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(ri);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if(!Add("PlaneXY", material_instance[0], glm::quat(1.0f, 0.0f, 0.0f, 0.0f)))
            return false;

        const float rot90 = glm::radians(90.0f);
        if(!Add("PlaneYZ", material_instance[1], glm::angleAxis(rot90, glm::vec3(0.0f, 1.0f, 0.0f))))
            return false;
        if(!Add("PlaneXZ", material_instance[2], glm::angleAxis(rot90, glm::vec3(1.0f, 0.0f, 0.0f))))
            return false;

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
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
        ecs_context = GetECSContext();

        if(!ecs_context)
            return false;

        if(!InitScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    ~TestApp()
    {
        SAFE_CLEAR(geom_plane_grid);
    }

    bool Init() override
    {
        if(!InitMDP())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("PlaneGrid3D"),argc,argv,1280,720);
}

