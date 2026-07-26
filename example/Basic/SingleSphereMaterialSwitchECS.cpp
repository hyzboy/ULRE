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
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
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
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        gvf.Add(VertexSemantic::TexCoord, VF_V2F, 2, sizeof(float) * 2);
        gvf.Add(VertexSemantic::Normal, VF_V3F, 3, sizeof(float) * 3);
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

    MaterialProgram *near_material = nullptr;
    MaterialProgram *far_material = nullptr;

    DeviceBuffer *mi_ssbo = nullptr;
    graph::mtl::SSBOType material_ssbo_type = graph::mtl::SSBOType::UserDefined;
    uint32_t material_ssbo_id = 0;
    uint32_t material_ssbo_stride = 0;

    Texture2DArray *near_base_color_array = nullptr;
    Texture2DArray *near_normal_array = nullptr;
    Texture2D *far_base_color_texture = nullptr;
    Texture2D *far_normal_texture = nullptr;
    Sampler *sampler = nullptr;

    BindlessTextureManager *bindless_texture_manager = nullptr;

    VertexDataManager *mesh_vdm = nullptr;
    Geometry *sphere_geometry = nullptr;
    Primitive *sphere_primitive = nullptr;

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
        auto *render_context = GetRenderContext();
        if (!render_context)
            return LogFail("InitTextures", "render context is null");

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return LogFail("InitTextures", "graphics context is null");

        auto *texture_manager = graphics_context->GetTextureManager();
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
        auto *render_context = GetRenderContext();
        if (!render_context)
            return LogFail("InitMaterials", "render context is null");

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return LogFail("InitMaterials", "graphics context is null");

        auto *material_manager = graphics_context->GetMaterialManager();
        auto *sampler_manager = graphics_context->GetSamplerManager();
        auto *device = graphics_context->GetDevice();
        if (!material_manager || !sampler_manager || !device)
            return LogFail("InitMaterials", "required manager/device is null");

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles,
                                        mtl::WithCamera::With,
                                        mtl::WithLocalToWorld::With,
                                        mtl::WithSky::With);
        near_material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::StandardTextureArray, &cfg);
        far_material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::Standard, &cfg);
        if (!near_material || !far_material)
            return LogFail("InitMaterials", "failed to create near/far materials");

        if (near_material->GetMIDataBytes() != far_material->GetMIDataBytes())
            return LogFail("InitMaterials", "near/far MI layout mismatch");

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return LogFail("InitMaterials", "failed to create sampler");

        if (!bindless_texture_manager)
        {
            bindless_texture_manager = new BindlessTextureManager();
            if (!bindless_texture_manager->Init(VkDevice(*device)))
                return LogFail("InitMaterials", "failed to init bindless texture manager");

            render_context->SetBindlessTextureManager(bindless_texture_manager);
            material_manager->SetBindlessLayout(bindless_texture_manager->GetLayout());
        }

        return true;
    }

    bool InitGeometry()
    {
        auto *render_context = GetRenderContext();
        if (!render_context)
            return LogFail("InitGeometry", "render context is null");

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return LogFail("InitGeometry", "graphics context is null");

        auto *buffer_manager = graphics_context->GetBufferManager();
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

        graph::mtl::MaterialRecipe recipe{};
        recipe.recipe_name = use_far_material ? "06e.SingleSphereSwitch.Far" : "06e.SingleSphereSwitch.Near";
        recipe.shading_model = graph::mtl::ShadingModel::Standard;
        recipe.preset_hint = static_cast<uint32_t>(use_far_material
                                                 ? graph::mtl::MaterialPreset::Standard
                                                 : graph::mtl::MaterialPreset::StandardTextureArray);
        recipe.domain = "06e.SingleSphereSwitch";
        sphere_primitive_component->SetMaterialRecipe(recipe);

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

        sphere_primitive_component->SetMaterialStructResource(graph::mtl::DataSlot::PBRSurface,
                                                              material_ssbo_type,
                                                              material_ssbo_id,
                                                              mi_ssbo,
                                                              1,
                                                              material_ssbo_stride,
                                                              true);
        return true;
    }

    bool InitRenderResources()
    {
        if (!near_material || !far_material || !near_base_color_array || !near_normal_array || !far_base_color_texture || !far_normal_texture)
            return LogFail("InitRenderResources", "required material/texture is null");

        auto *render_context = GetRenderContext();
        if (!render_context)
            return LogFail("InitRenderResources", "render context is null");

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return LogFail("InitRenderResources", "graphics context is null");

        auto *buffer_manager = graphics_context->GetBufferManager();
        auto *primitive_manager = graphics_context->GetPrimitiveManager();
        if (!buffer_manager || !primitive_manager)
            return LogFail("InitRenderResources", "required runtime manager is null");

        const uint32_t mi_data_bytes = near_material->GetMIDataBytes();
        if (mi_data_bytes == 0 || mi_data_bytes != sizeof(mtl::StandardMaterialInstance))
            return LogFail("InitRenderResources", "unexpected MI struct size");
        material_ssbo_stride = mi_data_bytes;

        bool has_struct_binding = false;
        for (const auto &req : near_material->GetMaterialResourceLayout().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            material_ssbo_type = req.ssbo_type;
            material_ssbo_id = req.ssbo_id;
            break;
        }
        if (!has_struct_binding)
            return LogFail("InitRenderResources", "material has no MI contract binding");

        mtl::StandardMaterialInstance mi_data{};
        mi_data.base_color = PackRGBA8Float(0.72f, 0.72f, 0.72f, 1.0f);
        mi_data.metallic = 0.15f;
        mi_data.roughness = 0.25f;
        mi_data.normal_scale = 0.35f;

        mi_ssbo = buffer_manager->CreateSSBO("SingleSphereSwitch:MIData", mi_data_bytes, nullptr, SharingMode::Exclusive);
        if (!mi_ssbo)
            return LogFail("InitRenderResources", "create MI SSBO failed");

        auto *mi_gpu = mi_ssbo->GetGPUBuffer();
        if (!mi_gpu)
            return LogFail("InitRenderResources", "MI GPU buffer is null");

        auto *mi_dst = static_cast<uint8_t *>(mi_gpu->Map(0, mi_data_bytes));
        if (!mi_dst)
            return LogFail("InitRenderResources", "map MI SSBO failed");
        memcpy(mi_dst, &mi_data, mi_data_bytes);
        mi_gpu->Unmap();

        sphere_primitive = primitive_manager->CreatePrimitive(sphere_geometry, near_material, nullptr, nullptr);
        if (!sphere_primitive)
            return LogFail("InitRenderResources", "create primitive failed");

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

        sphere_primitive_component->SetPrimitive(sphere_primitive);
        sphere_primitive_component->RequestPipeline(InlinePipeline::Solid3D);
        sphere_primitive_component->SetVisible(true);

        if (!ApplyMaterialMode(false))
            return false;

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
        SAFE_CLEAR(sphere_primitive)
        SAFE_CLEAR(sphere_geometry)
        SAFE_CLEAR(mesh_vdm)

        SAFE_CLEAR(mi_ssbo)

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
