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
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/color/ColorPacking.h>
#include<hgl/log/Log.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/single_sphere_material_switch.material_data.toml";
    constexpr uint32 MATERIAL_DATA_ROW_NEAR = 0;
    constexpr uint32 MATERIAL_DATA_ROW_FAR  = 1;
}

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

// 两套材质的纹理都在配方文件里声明：
//   ShaderLibrary/asset/recipe/Examples/Material/06e.SingleSphereSwitch.Near.recipe.toml
//     ⇒ texarray 数组 URI（规约：ShaderLibrary/asset/texarray/single_sphere_near_*.texarray.toml）
//   ShaderLibrary/asset/recipe/Examples/Material/06e.SingleSphereSwitch.Far.recipe.toml
//     ⇒ 同一份文件的**单张 2D 纹理**（配方按平铺语义采样，不需要数组）
// 原来这里的 `PBR_FOLDER` 常量、`BuildTexturePair` 探测函数与四个纹理 URI 常量已删除。

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

    graph::mtl::MaterialRecipe near_recipe{};
    graph::mtl::MaterialRecipe far_recipe{};

    graph::GlobalSSBOBinding near_material_data_ssbo_accessor{};
    graph::GlobalSSBOBinding far_material_data_ssbo_accessor{};

    /// 材质授权统一走配方：纹理句柄由 `MaterialRecipeRegistry` 按配方文件里的
    /// `[[textures]]` 自动创建，示例不再持有 TextureAsset 指针。

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

    bool InitTextures()
    {
        // near/far 两套纹理都在配方文件的 `[[textures]]` 里声明（near 走 texarray 数组 URI，
        // far 走文件 URI）。far 的句柄由注册表创建后停在 MetaOnly，而远距材质在第 1 帧
        // 就可能被切到，不能等 TextureStreamingSystem 异步完成 ⇒ 这里显式同步读盘。

        for (mtl::RecipeTextureBinding &binding : far_recipe.textures)
        {
            if (!binding.asset)
                return LogFail("InitTextures", "far recipe texture handle is null");
            if (!binding.asset->LoadPayload() || !binding.asset->GetTexture())
                return LogFail("InitTextures", "failed to load far material texture");
        }

        return true;
    }

    bool InitMaterials()
    {
        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW_NEAR, near_material_data_ssbo_accessor)
         || !GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW_FAR,  far_material_data_ssbo_accessor))
            return LogFail("InitMaterials", "material data block binding failed");

        const mtl::MaterialRecipe *near_loaded_recipe =
            GetRecipe("06e.SingleSphereSwitch.Near");
        const mtl::MaterialRecipe *far_loaded_recipe =
            GetRecipe("06e.SingleSphereSwitch.Far");
        if (!near_loaded_recipe || !far_loaded_recipe)
            return LogFail("InitMaterials", "material recipe not found");
        near_recipe = *near_loaded_recipe;
        if (!(near_recipe.material_ssbo_binding = near_material_data_ssbo_accessor).IsValid())
            return LogFail("InitMaterials", "near material SSBO binding failed");

        far_recipe = *far_loaded_recipe;
        if (!(far_recipe.material_ssbo_binding = far_material_data_ssbo_accessor).IsValid())
            return LogFail("InitMaterials", "far material SSBO binding failed");

        // 纹理是材质原型固有属性 ⇒ 两套配方各自在文件里声明，切换时只换配方即可。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。

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

        // 共享顶点池：几何写进 mesh_vdm，由 GeometryAssetRegistry 按 URI 取得
        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        sphere_geometry = GetGeometry("geo:builtin:sphere_64");
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
        sphere_struct = material_data_ssbo_accessor;
        sphere_material_data->SetDataResource(sphere_struct);
        return true;
    }

    bool InitRenderResources()
    {
        if (!near_material_data_ssbo_accessor
         || !far_material_data_ssbo_accessor)
            return LogFail("InitRenderResources", "SSBO not allocated");

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
        // 几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        sphere_geometry = nullptr;

        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitMaterials())
            return false;
        if (!InitTextures())
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
