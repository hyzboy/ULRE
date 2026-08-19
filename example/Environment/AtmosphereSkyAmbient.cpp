// AtmosphereSkyAmbient — 同一个 sky light 算法双用途演示
// ① 天空球向内显示（SkyMinimal——Position 方向即法线）
// ② 低画质间接光 ambient：物体世界法线采样同一算法（EvalSkyAtmosphere）
//    ——无 GI/IBL 探针时 sky ambient 参与 PBR 计算（indirect_sky_ambient）
#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>

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

class TestApp : public WorkObject
{
private:

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    struct RenderMesh
    {
        Geometry* geometry = nullptr;
        PrimitiveAsset asset{};

        ~RenderMesh() { delete geometry; }
    };

    graph::mtl::MaterialRecipe sky_recipe{};
    graph::mtl::MaterialRecipe mesh_recipe{};
    graph::SSBOArrayAccessor<ssbo::LitMaterialData>* mtl_data_ssbo_accessor = nullptr;

    Geometry* prim_sky_sphere = nullptr;
    PrimitiveAsset sky_asset{};

    Texture2D* base_texture = nullptr;
    Texture2D* normal_texture = nullptr;
    Texture2D* roughness_texture = nullptr;
    Sampler* sampler = nullptr;

    std::vector<std::unique_ptr<RenderMesh>> meshes;

private:

    bool InitMaterial()
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();

        if (!texture_manager || !sampler_manager)
            return false;

        mesh_recipe.recipe_name = "AtmosphereSkyAmbient.Lit";
        mesh_recipe.mtl_def_id = "Lit";
        mesh_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        mesh_recipe.domain = "AtmosphereSkyAmbient";
        graph::mtl::UpsertRecipeSSBOAssetBinding(mesh_recipe,
                                                 graph::mtl::DefaultMaterialDataSlotName,
                                                 mtl_data_ssbo_accessor->GetSSBOBinding());

        base_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        if (!base_texture)
            return false;

        normal_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        if (!normal_texture)
            return false;

        roughness_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);
        if (!roughness_texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        return true;
    }

    bool InitMISSBO()
    {
        auto* domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        ssbo::LitMaterialData material_data{};
        material_data.base_color   = Color4f(1.0f);
        material_data.metallic     = 0.08f;
        material_data.roughness    = 0.92f;
        material_data.normal_scale = 0.35f;

        mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<ssbo::LitMaterialData>(
            graph::mtl::SSBOType::PBRSurface,
            "AtmosphereSkyAmbient:PBRSurface:MaterialData",
            1);
        if (!mtl_data_ssbo_accessor)
            return false;

        (*mtl_data_ssbo_accessor)[0] = material_data;
        mtl_data_ssbo_accessor->Commit();
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
        HexSphereCreateInfo hsci;
        hsci.subdivisions = 3;
        hsci.radius = 256;

        prim_sky_sphere = create_geometry(CreateSkyGeometryVertexFormat(),
            [&](GeometryCreater* pc) { return CreateHexSphere(pc, &hsci); });
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

            auto mesh = std::make_unique<RenderMesh>();
            mesh->geometry = geom;
            mesh->asset = PrimitiveAsset(geom, &mesh_recipe, PrimitiveType::Triangles);
            meshes.push_back(std::move(mesh));
            return true;
        };

        if (!add_mesh([](GeometryCreater* pc) { return CreateSphere(pc, 48); }))
            return false;

        {
            CubeCreateInfo cci;
            cci.segments_x = 1;
            cci.segments_y = 1;
            cci.segments_z = 1;
            if (!add_mesh([&](GeometryCreater* pc) { return CreateCube(pc, &cci); }))
                return false;
        }

        {
            ConeCreateInfo cci;
            cci.radius = 1;
            cci.halfExtend = 1;
            cci.numberSlices = 48;
            cci.numberStacks = 3;
            if (!add_mesh([&](GeometryCreater* pc) { return CreateCone(pc, &cci); }))
                return false;
        }

        {
            TorusCreateInfo tci;
            tci.innerRadius = 0.8f;
            tci.outerRadius = 1.0f;
            tci.numberSlices = 48;
            tci.numberStacks = 12;
            if (!add_mesh([&](GeometryCreater* pc) { return CreateTorus(pc, &tci); }))
                return false;
        }

        {
            CapsuleCreateInfo cci;
            if (!add_mesh([&](GeometryCreater* pc) { return CreateCapsule(pc, &cci); }))
                return false;
        }

        return true;
    }

    bool InitRecipes()
    {
        if (!prim_sky_sphere)
            return false;

        sky_recipe.recipe_name = "AtmosphereSkyAmbient.Sky";
        sky_recipe.mtl_def_id = "SkyMinimal";
        sky_recipe.render_state_overrides.pipeline_config = mtl::MakeSkyConfig();
        sky_recipe.domain = "AtmosphereSkyAmbient";

        return true;
    }

    bool InitSceneEntities()
    {
        // 天空球（固定原点）
        auto* sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
        auto sky_transform = sky_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto sky_prim = sky_entity->AddComponent<PrimitiveComponent>();

        sky_transform->SetLocalPosition(glm::vec3(0.0f));
        sky_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        sky_transform->SetLocalScale(glm::vec3(1.0f));
        sky_transform->SetMovable(false);

        sky_prim->SetPrimitiveAsset(&sky_asset);
        sky_prim->SetVisible(true);

        // 物体群（环绕分布——法线方向各不相同 → sky ambient 方向采样差异可见）
        const size_t count = meshes.size();
        size_t index = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();

            auto* entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(4.5f, 0.0f, 0.0f));
            pos.y = 1.0f;   // 抬高——与 sun 方向有夹角

            transform->SetLocalPosition(pos);
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&rm->asset);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, base_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal, normal_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Roughness, roughness_texture, sampler);

            hgl::ecs::PrimitiveComponent::MaterialDataSlotAuthoringResource mesh_struct{};
            mesh_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            mesh_struct.ssbo_id = mtl_data_ssbo_accessor->GetSSBOId();
            mesh_struct.data_index = 0;
            mesh_struct.use_data_index = false;
            mesh_struct.shared_across_instances = false;
            primitive_comp->SetMaterialDataSlotResource(mesh_struct);
            primitive_comp->SetVisible(true);

            ++index;
        }

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        auto environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();
        if (!environment_system)
            return false;

        // 同一 SkyInfo——sky dome 显示与物体 ambient 采样共用
        auto* sky = environment_system->EditSkyInfo();
        if (sky)
        {
            glm::vec3 sun_dir = glm::normalize(glm::vec3(0.35f, 0.55f, 0.76f));
            sky->sun_direction = math::Vector4f(sun_dir.x, sun_dir.y, sun_dir.z, 0.0f);
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
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f);
        camera->distance = 10.0f;   // 在天空球（半径 256）内——看到物体 + 天空背景
        camera->yaw = 30.0f;
        camera->pitch = -15.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    bool Init() override
    {
        if (!InitMISSBO())
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
    return RunFramework<TestApp>(OS_TEXT("AtmosphereSkyAmbient"), argc, argv, 1280, 720);
}
