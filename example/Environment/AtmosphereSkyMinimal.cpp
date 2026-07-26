#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<memory>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    GeometryVertexFormat CreateSkyMinimalGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }
}

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *sky_entity = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    MaterialProgram *          mtl_sky_sphere      =nullptr;

    Geometry *          prim_sky_sphere     =nullptr;

private:

    bool InitMDP()
    {
        if (!prim_sky_sphere)
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        mtl::SkyMinimalCreateConfig cfg;

        mtl_sky_sphere = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::SkyMinimal,
                                                                  &cfg,
                                                                  prim_sky_sphere->GetGeometryVertexFormat());
        if (!mtl_sky_sphere)
            return false;

        return true;
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

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateSkyMinimalGeometryVertexFormat());

        struct HexSphereCreateInfo hsci;

        hsci.subdivisions=3;
        hsci.radius=256;

        prim_sky_sphere=CreateHexSphere(pc.get(),&hsci);
        if (prim_sky_sphere)
            geometry_manager->Add(prim_sky_sphere);

        return prim_sky_sphere;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
            return false;

        if(!prim_sky_sphere || !mtl_sky_sphere)
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        Primitive *ri=primitive_manager->CreatePrimitive(prim_sky_sphere, mtl_sky_sphere, nullptr, nullptr);
        if(!ri)
            return false;

        sky_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("SkySphere");
        auto transform = sky_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = sky_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(ri);
        graph::mtl::MaterialRecipe recipe{};
        recipe.recipe_name = "AtmosphereSkyMinimal.Sky";
        recipe.shading_model = graph::mtl::ShadingModel::Sky;
        recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::SkyMinimal);
        recipe.domain = "AtmosphereSkyMinimal";
        prim_comp->SetMaterialRecipe(recipe);
        prim_comp->RequestPipeline(InlinePipeline::Sky);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(10,10,10);
        camera->distance = 1.0f;
        camera->yaw = 0.0f;
        camera->pitch = 0.0f;
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

        auto environment_system = ecs_context->GetSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<hgl::ecs::EnvironmentSystem>();

        if (environment_system)
        {
            environment_system->EditSkyInfo();
            environment_system->SyncSkyUBO();
        }

        if(!InitECSScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    bool Init() override
    {
        if(!CreateRenderObject())
            return(false);

        if(!InitMDP())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("SimplestAtmosphere"),argc,argv,1280,720);
}
