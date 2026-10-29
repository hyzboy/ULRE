// NOTE (test-only):
// This sample exists only to validate runtime material switching behavior in isolation.
// It is NOT the production path for material LOD.
//
// Planned production path:
// 1) ShaderProgram LOD policy is configured in top-level material definitions.
// 2) Runtime selection/switching is performed automatically by ECS and/or GPUSCENE.
// 3) Application/demo code should not manually own final material LOD decision logic.

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/color/ColorPacking.h>
#include<hgl/log/Log.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
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

class SingleSphereMaterialSwitchApp : public WorkObject
{
private:

    ECSContext *ecs_world = nullptr;
    Entity *camera_entity = nullptr;
    Entity *sphere_entity = nullptr;

    hgl::ecs::TransformAccessor sphere_transform;
    hgl::ecs::GeometryData *sphere_primitive_component = nullptr;
    hgl::ecs::MaterialData *sphere_material_data = nullptr;

    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    graph::mtl::MaterialRecipe near_recipe{};
    graph::mtl::MaterialRecipe far_recipe{};

    MaterialDataAccessor near_material_data_ssbo_accessor{};
    MaterialDataAccessor far_material_data_ssbo_accessor{};

    Texture2DArray *near_base_color_array = nullptr;
    Texture2DArray *near_normal_array = nullptr;
    Texture2D *far_base_color_texture = nullptr;
    Texture2D *far_normal_texture = nullptr;

    /// 材质授权统一走句柄：上面四张纹理各有一个 TextureAsset 句柄（Resident，只因文件同步装配而成）。
    TextureAsset *near_base_color_asset = nullptr;
    TextureAsset *near_normal_asset = nullptr;
    TextureAsset *far_base_color_asset = nullptr;
    TextureAsset *far_normal_asset = nullptr;

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
                                                                       far_base_color_texture->GetMipLevel());
        near_normal_array = texture_manager->CreateTexture2DArray("single_sphere_normal_array",
                                                                   far_normal_texture->GetWidth(),
                                                                   far_normal_texture->GetHeight(),
                                                                   1,
                                                                   far_normal_texture->GetFormat(),
                                                                   far_normal_texture->GetMipLevel());
        if (!near_base_color_array || !near_normal_array)
            return LogFail("InitTextures", "failed to create 2DArray textures");

        if (!texture_manager->LoadTexture2DArray(near_base_color_array, 0, base_file))
            return LogFail("InitTextures", "failed to load baseColor into array layer 0");
        if (!texture_manager->LoadTexture2DArray(near_normal_array, 0, normal_file))
            return LogFail("InitTextures", "failed to load normal into array layer 0");

        near_base_color_asset  = texture_manager->WrapTextureAsset(near_base_color_array);
        near_normal_asset      = texture_manager->WrapTextureAsset(near_normal_array);
        far_base_color_asset   = texture_manager->WrapTextureAsset(far_base_color_texture);
        far_normal_asset       = texture_manager->WrapTextureAsset(far_normal_texture);

        if (!near_base_color_asset || !near_normal_asset || !far_base_color_asset || !far_normal_asset)
            return LogFail("InitTextures", "failed to wrap texture assets");

        return true;
    }

    bool InitMaterials()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return LogFail("InitMaterials", "domain manager null");

        near_material_data_ssbo_accessor =
            domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!near_material_data_ssbo_accessor)
            return LogFail("InitMaterials", "SSBO allocation failed");

        far_material_data_ssbo_accessor =
            domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!far_material_data_ssbo_accessor)
            return LogFail("InitMaterials", "SSBO allocation failed");

        const mtl::MaterialRecipe *near_loaded_recipe =
            GetRecipe("06e.SingleSphereSwitch.Near");
        const mtl::MaterialRecipe *far_loaded_recipe =
            GetRecipe("06e.SingleSphereSwitch.Far");
        if (!near_loaded_recipe || !far_loaded_recipe)
            return LogFail("InitMaterials", "material recipe not found");
        near_recipe = *near_loaded_recipe;
        if (!(near_recipe.material_ssbo_binding = near_material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return LogFail("InitMaterials", "near material SSBO binding failed");

        // 纹理是材质原型固有属性 ⇒ 两套配方各自声明，切换时只换配方即可。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。
        if (!near_recipe.AddTexture("base_color", near_base_color_asset, true)
         || !near_recipe.AddTexture("normal", near_normal_asset, true))
            return LogFail("InitMaterials", "near recipe texture binding failed");

        far_recipe = *far_loaded_recipe;
        if (!(far_recipe.material_ssbo_binding = far_material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return LogFail("InitMaterials", "far material SSBO binding failed");

        // 同名 upsert 覆盖成 Far 那对纹理（2D 句柄，身份为 `texid:`）
        if (!far_recipe.AddTexture("base_color", far_base_color_asset, true)
         || !far_recipe.AddTexture("normal", far_normal_asset, true))
            return LogFail("InitMaterials", "far recipe texture binding failed");

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

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
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
        if (use_far_material == far_mode && sphere_material_data && sphere_material_data->HasRecipeOverride())
            return true;

        use_far_material = far_mode;

        if (!sphere_primitive_component)
            return true;

        const auto &material_data_ssbo_accessor =
            use_far_material
                ? far_material_data_ssbo_accessor
                : near_material_data_ssbo_accessor;
        if (!material_data_ssbo_accessor)
            return false;

        sphere_material_data->SetRecipe(use_far_material ? far_recipe : near_recipe);

        // 纹理随配方一起切换（两套配方各自持有自己的纹理绑定），实体端不再单独授权纹理
        hgl::ecs::MaterialData::MaterialDataAuthoringResource sphere_struct{};
        sphere_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
        sphere_material_data->SetDataResource(sphere_struct);
        return true;
    }

    bool InitRenderResources()
    {
        if (!near_base_color_array || !near_normal_array || !far_base_color_texture || !far_normal_texture)
            return LogFail("InitRenderResources", "required material/texture is null");

        if (!near_material_data_ssbo_accessor
         || !far_material_data_ssbo_accessor)
            return LogFail("InitRenderResources", "SSBO not allocated");

        graph::ssbo::PBRSurfaceRow material_data{};
        material_data.base_color = Color4f(0.72f, 0.72f, 0.72f, 1.0f);
        material_data.metallic = 0.15f;
        material_data.roughness = 0.25f;
        material_data.normal_scale = 0.35f;

        if (!near_material_data_ssbo_accessor.Write(material_data)
         || !far_material_data_ssbo_accessor.Write(material_data))
            return LogFail("InitRenderResources", "write material data failed");

        sphere_asset = PrimitiveAsset(sphere_geometry, &near_recipe, PrimitiveType::Triangles);

        return true;
    }

    bool EnsureCameraSystem()
    {
        if (!ecs_world)
            return LogFail("EnsureCameraSystem", "ECS context is null");

        auto camera_system = ecs_world->EnsureCameraSystem();

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

        auto transform = ecs_world->GetTransform(ecs_world->CreateTransform(sphere_entity->GetEntityID(), Mobility::Movable));
        auto primitive_component = sphere_entity->GetContext()->GetOrCreateGeometryData(sphere_entity);
        if (!transform.IsValid() || !primitive_component)
            return LogFail("InitECSScene", "create sphere components failed");

        sphere_transform = transform;
        sphere_primitive_component = primitive_component;
        sphere_material_data = ecs_world->GetOrCreateMaterialData(sphere_entity);

        sphere_transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        sphere_transform.SetLocalScale(glm::vec3(1.6f, 1.6f, 1.6f));
        sphere_transform.SetMobility(Mobility::Movable);

        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        if (!ApplyMaterialMode(false))
            return false;

        sphere_primitive_component->GetOwner()->GetContext()->GetOrCreateGeometryData(sphere_primitive_component->GetOwnerID())->SetPrimitiveAsset(&sphere_asset);

        return true;
    }

    bool InitCamera()
    {
        if (!EnsureCameraSystem())
            return LogFail("InitCamera", "camera system init failed");

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        if (!camera_entity)
            return LogFail("InitCamera", "create camera entity failed");

        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_world->GetOrCreateCamera(camera_entity);
        if (!camera)
            return LogFail("InitCamera", "create camera component failed");

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 8.0f;
        camera->min_distance = 2.0f;
        camera->max_distance = 20.0f;
        camera->yaw = 20.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~SingleSphereMaterialSwitchApp()
    {
        SAFE_CLEAR(sphere_geometry)
        SAFE_CLEAR(mesh_vdm)

        SAFE_CLEAR(near_base_color_array)
        SAFE_CLEAR(near_normal_array)
        SAFE_CLEAR(far_base_color_texture)
        SAFE_CLEAR(far_normal_texture)
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

        if (sphere_transform.IsValid())
        {
            const float angle = static_cast<float>(elapsed_time) * 0.35f;
            sphere_transform.SetLocalRotation(glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f)));
        }

        auto *camera_component = camera_entity ? ecs_world->GetCameraByEntity(camera_entity) : nullptr;

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
    return RunFramework<SingleSphereMaterialSwitchApp>(OS_TEXT("Single Sphere ShaderProgram Switch"), argc, argv, 1280, 720);
}
