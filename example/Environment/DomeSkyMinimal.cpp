#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/PrimitiveMaterialSlot.h>
#include<memory>

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
    const VertexFormatMap kSkyVertexFormats = {
        {VAN::Position, PF_RGB32F},
    };
}

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *sky_entity = nullptr;
    hgl::ecs::Entity *ground_entity = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    MaterialTemplate *          mtl_sky_sphere      = nullptr;
    const VIL *                 sky_vil             = nullptr;
    PrimitiveMaterialSlot       sky_slot;

    Geometry *          prim_sky_dome       = nullptr;
    Geometry *          prim_ground_plane   = nullptr;

private:

    bool InitMDP()
    {

        static const mtl::MaterialAssetRecord kSkyCfg {
            .id       = "dome_sky_minimal",
            .preset   = mtl::MaterialPreset::SkyMinimal,
            .l2w      = false,
            .sky      = true,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };
        auto *registry = GetMaterialAssetRegistry();
        auto *material_manager = GetMaterialManager();
        if (!registry || !material_manager)
            return false;

        auto handle = registry->Acquire(kSkyCfg);
        if (!handle.IsValid() || !handle.material)
            return false;

        mtl_sky_sphere = handle.material;
        sky_vil = registry->ResolveVIL(handle.material, kSkyCfg);
        if (!sky_vil)
            return false;

        sky_slot = material_manager->AllocMaterialInstanceSlot(handle.domain);
        if (!sky_slot.domain)
            return false;

        sky_slot.material_template        = handle.material;
        sky_slot.vil                      = sky_vil;
        sky_slot.preset                   = kSkyCfg.pipeline;
        sky_slot.texture_array_slot_flags = handle.material->GetTextureArraySlotFlags();

        if (!sky_slot.IsValid())
            return false;

        return true;
    }

    bool CreateRenderObject()
    {

        auto* device = GetDevice();
        auto* geometry_manager = GetGeometryManager();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        {
            auto pc = std::make_unique<GeometryCreater>(device, kSkyVertexFormats);

            DomeCreateInfo dci;
            dci.number_slices = 64;
            dci.inside_out = true;
            dci.normal = false;
            dci.tangent = false;
            dci.tex_coord = false;

            prim_sky_dome = CreateDome(pc.get(), &dci);
            if (!prim_sky_dome)
                return false;

            geometry_manager->Add(prim_sky_dome);
        }

        {
            auto pc = std::make_unique<GeometryCreater>(device, kSkyVertexFormats);

            prim_ground_plane = CreatePlaneSqaure(pc.get());
            if (!prim_ground_plane)
                return false;

            geometry_manager->Add(prim_ground_plane);
        }

        return true;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
            return false;

        if(!prim_sky_dome || !prim_ground_plane || !sky_slot.IsValid())
            return false;

        auto* primitive_manager = GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        Primitive *sky_prim = primitive_manager->CreatePrimitive(prim_sky_dome, sky_slot);
        if(!sky_prim)
            return false;

        sky_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("SkyDome");
        auto sky_transform = sky_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto sky_prim_comp = sky_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        sky_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        sky_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        sky_transform->SetLocalScale(glm::vec3(256.0f, 256.0f, 256.0f));
        sky_transform->SetMovable(false);

        sky_prim_comp->SetPrimitive(sky_prim);
        sky_prim_comp->SetVisible(true);

        Primitive *ground_prim = primitive_manager->CreatePrimitive(prim_ground_plane, sky_slot);
        if(!ground_prim)
            return false;

        ground_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("GroundPlane");
        auto ground_transform = ground_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto ground_prim_comp = ground_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        ground_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, -0.01f));
        ground_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        ground_transform->SetLocalScale(glm::vec3(256.0f, 256.0f, 1.0f));
        ground_transform->SetMovable(false);

        ground_prim_comp->SetPrimitive(ground_prim);
        ground_prim_comp->SetVisible(true);

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
        if(!InitMDP())
            return false;

        if(!CreateRenderObject())
            return false;

        if(!InitECS())
            return false;

        return true;
    }
};

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("DomeSkyMinimal"),argc,argv,1280,720);
}

