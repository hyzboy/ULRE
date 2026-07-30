// NOTE (test-only):
// This sample exists only to validate runtime material switching behavior in isolation.
// It is NOT the production path for material LOD.
//
// Planned production path:
// 1) MaterialProgram LOD policy is configured in top-level material definitions.
// 2) Runtime selection/switching is performed automatically by ECS and/or GPUSCENE.
// 3) Application/demo code should not manually own final material LOD decision logic.

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/color/ColorPacking.h>
#include<hgl/log/Log.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

constexpr const os_char PBR_FOLDER[] = OS_TEXT("Concrete_Plain");
static constexpr float SWITCH_NEAR_ENTER_DISTANCE = 7.0f;   // far->near
static constexpr float SWITCH_FAR_ENTER_DISTANCE = 9.0f;    // near->far

class TestApp : public WorkObject
{
private:

    ECSContext *ecs_world = nullptr;
    Entity *camera_entity = nullptr;
    Entity *sphere_entity = nullptr;

    CameraComponent *camera_component = nullptr;
    TransformComponent *sphere_transform = nullptr;
    PrimitiveComponent *sphere_primitive_component = nullptr;

    graph::mtl::MaterialRecipe near_recipe{};
    graph::mtl::MaterialRecipe far_recipe{};

    graph::SSBOArrayAccessor<mtl::StandardMaterialInstance>* mi_ssbo_accessor = nullptr;

    Texture2DArray *near_base_color_array = nullptr;
    Texture2DArray *near_normal_array = nullptr;
    Texture2D *far_base_color_texture = nullptr;
    Texture2D *far_normal_texture = nullptr;
    Sampler *sampler = nullptr;

    BindlessTextureManager *bindless_texture_manager = nullptr;

    VertexDataManager *mesh_vdm = nullptr;
    Geometry *sphere_geometry = nullptr;
    PrimitiveAsset sphere_asset{};

    bool use_far_material = false;
    double elapsed_time = 0.0;

private:

    bool LogFail(const char *stage, const char *reason)
    {
        GLogError("[SingleSphereMaterialSwitchECS] %s failed: %s", stage, reason);
        return false;
    }

    bool BuildTexturePair(OSString &base, OSString &normal) const
    {
        const OSString folder = filesystem::JoinPathWithFilename(OS_TEXT("res/image/pbr"), PBR_FOLDER);
        base = filesystem::JoinPathWithFilename(folder, OS_TEXT("baseColor.Tex2D"));
        normal = filesystem::JoinPathWithFilename(folder, OS_TEXT("normal.Tex2D"));
        if (!filesystem::FileExist(normal))
            normal = filesystem::JoinPathWithFilename(folder, OS_TEXT("Normal.Tex2D"));

        return filesystem::FileExist(base) && filesystem::FileExist(normal);
    }

    bool InitTextures()
    {
        auto *texture_manager = GetManager<TextureManager>();
        if (!texture_manager)
            return LogFail("InitTextures", "texture manager is null");

        OSString base_file;
        OSString normal_file;
        if (!BuildTexturePair(base_file, normal_file))
            return LogFail("InitTextures", "missing baseColor/normal texture pair");

        far_base_color_texture = texture_manager->LoadTexture2D(base_file, true);
        far_normal_texture = texture_manager->LoadTexture2D(normal_file, true);
        if (!far_base_color_texture || !far_normal_texture)
            return LogFail("InitTextures", "failed to load 2D textures for far material");

        near_base_color_array = texture_manager->CreateTexture2DArray("single_sphere_base_array",
                                                                       far_base_color_texture->GetWidth(),
                                                                       far_base_color_texture->GetHeight(),
                                                                       1,
                                                                       far_base_color_texture->GetFormat(),
                                                                       true);
        near_normal_array = texture_manager->CreateTexture2DArray("single_sphere_normal_array",
                                                                   far_normal_texture->GetWidth(),
                                                                   far_normal_texture->GetHeight(),
                                                                   1,
                                                                   far_normal_texture->GetFormat(),
                                                                   true);
        if (!near_base_color_array || !near_normal_array)
            return LogFail("InitTextures", "failed to create 2DArray textures");

        if (!texture_manager->LoadTexture2DArray(near_base_color_array, 0, base_file))
            return LogFail("InitTextures", "failed to load baseColor into array layer 0");
        if (!texture_manager->LoadTexture2DArray(near_normal_array, 0, normal_file))
            return LogFail("InitTextures", "failed to load normal into array layer 0");

        return true;
    }

    bool InitMaterials()
    {
        auto *sampler_manager = GetManager<SamplerManager>();
        if (!sampler_manager)
            return LogFail("InitMaterials", "required manager/device is null");

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return LogFail("InitMaterials", "failed to create sampler");

        auto* domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return LogFail("InitMaterials", "domain manager null");
        mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<mtl::StandardMaterialInstance>(
            graph::mtl::SSBOType::PBRSurface, "SingleSphereSwitch:MIData", 1);
        if (!mi_ssbo_accessor)
            return LogFail("InitMaterials", "SSBO allocation failed");

        near_recipe.recipe_name = "06e.SingleSphereSwitch.Near";
        graph::mtl::SetRecipePreset(near_recipe, graph::mtl::MaterialPreset::StandardTextureArray);
        near_recipe.domain = "06e.SingleSphereSwitch";
        graph::mtl::UpsertRecipeSSBOAssetBinding(near_recipe,
                                                 graph::mtl::SBS_MaterialInstance.name,
                                                 mi_ssbo_accessor->GetSSBOBinding());

        far_recipe = near_recipe;
        far_recipe.recipe_name = "06e.SingleSphereSwitch.Far";
        graph::mtl::SetRecipePreset(far_recipe, graph::mtl::MaterialPreset::Standard);

        return true;
    }

    bool InitGeometry()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return LogFail("InitGeometry", "buffer manager is null");

        mesh_vdm = new VertexDataManager(buffer_manager, CreateStandardGeometryVertexFormat());
        if (!mesh_vdm)
            return LogFail("InitGeometry", "failed to create vertex data manager");

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
            return LogFail("InitGeometry", "failed to init vertex data manager");

        GeometryCreater *pc = new GeometryCreater(mesh_vdm);
        if (!pc)
            return LogFail("InitGeometry", "failed to create geometry creator");

        sphere_geometry = inline_geometry::CreateSphere(pc, 64);
        delete pc;
        if (!sphere_geometry)
            return LogFail("InitGeometry", "failed to create sphere geometry");

        return true;
    }

    bool ApplyMaterialMode(const bool far_mode)
    {
        if (use_far_material == far_mode && sphere_primitive_component && sphere_primitive_component->HasMaterialRecipe())
            return true;

        use_far_material = far_mode;

        if (!sphere_primitive_component)
            return true;

        sphere_primitive_component->SetMaterialRecipe(use_far_material ? far_recipe : near_recipe);

        if (use_far_material)
        {
            sphere_primitive_component->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor,
                                                                  far_base_color_texture,
                                                                  sampler,
                                                                  PrimitiveComponent::MaterialTextureResourceKind::Texture2D);
            sphere_primitive_component->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal,
                                                                  far_normal_texture,
                                                                  sampler,
                                                                  PrimitiveComponent::MaterialTextureResourceKind::Texture2D);
        }
        else
        {
            sphere_primitive_component->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor,
                                                                  near_base_color_array,
                                                                  sampler,
                                                                  PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray);
            sphere_primitive_component->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal,
                                                                  near_normal_array,
                                                                  sampler,
                                                                  PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray);
        }

        hgl::ecs::PrimitiveComponent::MaterialStructNamedAuthoringResource sphere_struct{};
        sphere_struct.ssbo_name = graph::mtl::SBS_MaterialInstance.name;
        sphere_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
        sphere_struct.struct_index = 0;
        sphere_struct.use_struct_index = false;
        sphere_struct.shared_across_instances = false;
        sphere_primitive_component->SetMaterialStructResource(sphere_struct);
        return true;
    }

    bool InitRenderResources()
    {
        if (!near_base_color_array || !near_normal_array || !far_base_color_texture || !far_normal_texture)
            return LogFail("InitRenderResources", "required material/texture is null");

        if (!mi_ssbo_accessor)
            return LogFail("InitRenderResources", "SSBO not allocated");

        mtl::StandardMaterialInstance mi_data{};
        mi_data.base_color = PackRGBA8Float(0.72f, 0.72f, 0.72f, 1.0f);
        mi_data.metallic = 0.15f;
        mi_data.roughness = 0.25f;
        mi_data.normal_scale = 0.35f;

        (*mi_ssbo_accessor)[0] = mi_data;
        mi_ssbo_accessor->Commit();
        sphere_asset = PrimitiveAsset(sphere_geometry, &near_recipe, PrimitiveType::Triangles);

        return true;
    }

    bool EnsureCameraSystem()
    {
        if (!ecs_world)
            return LogFail("EnsureCameraSystem", "ECS context is null");

        auto camera_system = ecs_world->GetSystem<CameraSystem>();
        if (!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);
            if (ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    bool InitECSScene()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
            return LogFail("InitECSScene", "ECS context is null");

        if (!InitRenderResources())
            return false;

        sphere_entity = ecs_world->CreateEntity<Entity>("SwitchSphere");
        if (!sphere_entity)
            return LogFail("InitECSScene", "create sphere entity failed");

        auto transform = sphere_entity->AddComponent<TransformComponent>(Mobility::Movable);
        auto primitive_component = sphere_entity->AddComponent<PrimitiveComponent>();
        if (!transform || !primitive_component)
            return LogFail("InitECSScene", "create sphere components failed");

        sphere_transform = transform.get();
        sphere_primitive_component = primitive_component.get();

        sphere_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        sphere_transform->SetLocalScale(glm::vec3(1.6f, 1.6f, 1.6f));
        sphere_transform->SetMovable(true);

        sphere_primitive_component->RequestPipeline(InlinePipeline::Solid3D);
        sphere_primitive_component->SetVisible(true);

        // ApplyMaterialMode must be called BEFORE SetPrimitiveAsset.
        // If SetPrimitiveAsset is called first, HasMaterialRecipe() returns true via the
        // asset's recipe, causing ApplyMaterialMode's early-return to skip texture/struct setup.
        if (!ApplyMaterialMode(false))
            return false;

        sphere_primitive_component->SetPrimitiveAsset(&sphere_asset);

        return true;
    }

    bool InitCamera()
    {
        if (!EnsureCameraSystem())
            return LogFail("InitCamera", "camera system init failed");

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        if (!camera_entity)
            return LogFail("InitCamera", "create camera entity failed");

        auto camera = camera_entity->AddComponent<CameraComponent>();
        if (!camera)
            return LogFail("InitCamera", "create camera component failed");

        camera_component = camera.get();
        camera_component->control_mode = CameraComponent::ControlMode::ViewModel;
        camera_component->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera_component->distance = 8.0f;
        camera_component->min_distance = 2.0f;
        camera_component->max_distance = 20.0f;
        camera_component->yaw = 20.0f;
        camera_component->pitch = -20.0f;
        camera_component->is_main_camera = true;
        camera_component->matrix_dirty = true;

        camera_component->camera_data = GetCamera();
        camera_component->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera_component->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(sphere_geometry)
        SAFE_CLEAR(mesh_vdm)

        SAFE_CLEAR(mi_ssbo_accessor)

        SAFE_CLEAR(near_base_color_array)
        SAFE_CLEAR(near_normal_array)
        SAFE_CLEAR(far_base_color_texture)
        SAFE_CLEAR(far_normal_texture)

        if (bindless_texture_manager)
        {
            delete bindless_texture_manager;
            bindless_texture_manager = nullptr;
        }
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitTextures())
            return false;
        if (!InitMaterials())
            return false;
        if (!InitGeometry())
            return false;
        if (!InitECSScene())
            return false;
        if (!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed_time += delta_time;

        if (sphere_transform)
        {
            const float angle = static_cast<float>(elapsed_time) * 0.35f;
            sphere_transform->SetLocalRotation(glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f)));
        }

        if (camera_component)
        {
            const float distance = camera_component->distance;
            if (use_far_material)
            {
                if (distance < SWITCH_NEAR_ENTER_DISTANCE)
                    ApplyMaterialMode(false);
            }
            else
            {
                if (distance > SWITCH_FAR_ENTER_DISTANCE)
                    ApplyMaterialMode(true);
            }
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Single Sphere MaterialProgram Switch (ECS)"), argc, argv, 1280, 720);
}
