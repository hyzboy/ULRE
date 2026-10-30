// AtmosphereSkyAmbient — 同一个 sky light 算法双用途演示
// ① 天空球向内显示（SkyMinimal——Position 方向即法线）
// ② 低画质间接光 ambient：物体世界法线采样同一算法（EvalSkyAtmosphere）
//    ——无 GI/IBL 探针时 sky ambient 参与 PBR 计算（indirect_sky_ambient）
#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/LightSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<memory>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 天空球：只有 Position（球体——法线由 Position 方向推导，不存 Normal）
    GeometryVertexFormat CreateSkyGeometryVertexFormat()
    {
        return GeometryVertexFormat{
            {VertexSemantic::Position, VF_V3F},
        };
    }

    // 物体：发行矩阵（Position V3F + UV RG16F + Normal RG8——无切线）
    GeometryVertexFormat CreateMeshGeometryVertexFormat()
    {
        return GeometryVertexFormat{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},
            {VertexSemantic::Normal,   VF_V2UN8},
        };
    }
}

class AtmosphereSkyAmbientApp : public WorkObject
{
private:

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    struct MeshEntry
    {
        Geometry* geometry = nullptr;
        PrimitiveAsset asset{};
    };

    graph::mtl::MaterialRecipe sky_recipe{};
    graph::mtl::MaterialRecipe mesh_recipe{};
    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    MaterialDataAccessor material_data_ssbo_accessor{};

    Geometry* prim_sky_sphere = nullptr;
    PrimitiveAsset sky_asset{};

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitMaterial()
    {
        if (!material_data_ssbo_accessor)
            return false;

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("AtmosphereSkyAmbient.Lit");
        if (!loaded_recipe)
            return false;
        mesh_recipe = *loaded_recipe;
        if (!(mesh_recipe.material_ssbo_binding = material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return false;

        return true;
    }

    bool InitMaterialDataSSBO()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        graph::ssbo::PBRSurfaceRow material_data{};
        material_data.base_color   = Color4f(1.0f);
        material_data.metallic     = 0.08f;
        material_data.roughness    = 0.92f;
        material_data.normal_scale = 0.35f;

        material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return false;

        if (!material_data_ssbo_accessor.Write(material_data))
            return false;

        return true;
    }

    bool CreateRenderObjects()
    {
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return false;

        using namespace inline_geometry;

        auto create_geometry = [&](const GeometryVertexFormat& gvf, auto&& creator) -> Geometry*
        {
            auto pc = std::make_unique<GeometryCreater>(GetDevice(), gvf);
            if (!pc)
                return nullptr;
            return creator(pc.get());
        };

        // ── 天空球（SkyMinimal——仅 Position）──

        prim_sky_sphere = create_geometry(CreateSkyGeometryVertexFormat(),
            [&](GeometryCreater* pc) { return CreateGeometryPreset(pc, "hex_sphere_sky"); });
        if (!prim_sky_sphere)
            return false;

        geometry_manager->Add(prim_sky_sphere);
        sky_asset = PrimitiveAsset(prim_sky_sphere, &sky_recipe, PrimitiveType::Triangles);

        // ── 物体群（Lit——sky ambient 间接光）──
        const GeometryVertexFormat mesh_gvf = CreateMeshGeometryVertexFormat();

        auto add_mesh = [&](auto&& creator) -> bool
        {
            auto geom = create_geometry(mesh_gvf, creator);
            if (!geom)
                return false;

            geometry_manager->Add(geom);

            auto mesh = std::make_unique<MeshEntry>();
            mesh->geometry = geom;
            mesh->asset = PrimitiveAsset(geom, &mesh_recipe, PrimitiveType::Triangles);
            meshes.push_back(std::move(mesh));
            return true;
        };

        if (!add_mesh([](GeometryCreater* pc) { return CreateGeometryPreset(pc, "sphere_48"); }))
            return false;

        {
            if (!add_mesh([&](GeometryCreater* pc) { return CreateGeometryPreset(pc, "cube"); }))
                return false;
        }

        {
            if (!add_mesh([&](GeometryCreater* pc) { return CreateGeometryPreset(pc, "cone_low"); }))
                return false;
        }

        {
            if (!add_mesh([&](GeometryCreater* pc) { return CreateGeometryPreset(pc, "torus_low"); }))
                return false;
        }

        {
            if (!add_mesh([&](GeometryCreater* pc) { return CreateGeometryPreset(pc, "capsule"); }))
                return false;
        }

        return true;
    }

    bool InitRecipes()
    {
        if (!prim_sky_sphere)
            return false;

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("AtmosphereSkyAmbient.Sky");
        if (!loaded_recipe)
            return false;
        sky_recipe = *loaded_recipe;

        return true;
    }

    bool InitSceneEntities()
    {
        // 天空球（固定原点）
        auto* sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
        auto sky_transform = ecs_context->GetTransform(ecs_context->CreateTransform(sky_entity->GetEntityID(), Mobility::Static));
        auto sky_prim = sky_entity->GetContext()->GetOrCreateGeometryData(sky_entity);

        sky_transform.SetLocalPosition(glm::vec3(0.0f));
        sky_transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        sky_transform.SetLocalScale(glm::vec3(1.0f));
        sky_transform.SetMobility(Mobility::Static);

        sky_prim->GetOwner()->GetContext()->GetOrCreateGeometryData(sky_prim->GetOwnerID())->SetPrimitiveAsset(&sky_asset);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        // 物体群（环绕分布——法线方向各不相同 → sky ambient 方向采样差异可见）
        const size_t count = meshes.size();
        size_t index = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();

            auto* entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(count));
            // 水平环绕（xz 平面圆上 5 点）+ 高度 1.0——位置互不重叠；
            // 绕 y 轴旋转（法线方向各不相同 → sky ambient 方向采样差异可见）
            glm::quat rotation = glm::angleAxis(-angle, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 pos = glm::vec3(4.5f * glm::cos(angle), 1.0f, 4.5f * glm::sin(angle));

            transform.SetLocalPosition(pos);
            transform.SetLocalRotation(rotation);
            transform.SetLocalScale(glm::vec3(1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            // 纹理已由配方声明（mesh_recipe.AddTexture），实体端不再重复绑定

            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
            material_data_comp->SetDataResource(mesh_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

            ++index;
        }

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        auto light_system = ecs_context->GetSystem<LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<LightSystem>();
        if (!light_system)
            return false;

        // T6b：光源 = **世界私有光源行 0** —— sky dome 显示与物体 ambient 采样都从这一行取光
        //（天空只留外观：底色/光晕色，走本世界的 env profile）。
        {
            const glm::vec3 sun_dir = glm::normalize(glm::vec3(0.35f, 0.55f, 0.76f));
            light_system->SetMainLightDirection(math::Vector3f(sun_dir.x, sun_dir.y, sun_dir.z));
        }

        if (!InitSceneEntities())
            return false;

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f);
        camera->distance = 10.0f;   // 在天空球（半径 256）内——看到物体 + 天空背景
        camera->yaw = 30.0f;
        camera->pitch = -15.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~AtmosphereSkyAmbientApp()
    {
    }

    bool Init() override
    {
        if (!InitMaterialDataSSBO())
            return false;

        if (!InitMaterial())
            return false;

        if (!CreateRenderObjects())
            return false;

        if (!InitRecipes())
            return false;

        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!InitECS())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<AtmosphereSkyAmbientApp>(OS_TEXT("AtmosphereSkyAmbient"), argc, argv, 1280, 720);
}
