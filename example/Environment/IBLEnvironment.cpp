#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>

#include<hgl/graph/gizmo/SunDirectionControlSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<vector>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateSkyCubeGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }

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

#define DRAW_SKY_SPHERE
#define DRAW_GIZMO

/**
 * IBL 环境光照示例:
 *
 * 以 sky_34 全景环境(cubemap)为天空背景,对场景物体做基于图像的光照(split-sum)。
 * IBL 资源由 TexConv 转换产出:
 *   * <env>_irradiance.TexCube  漫反射辐照度(按世界法线采样)
 *   * <env>_prefilter.TexCube   镜面预滤波(按反射向量采样)
 *   * brdflut.Tex2D             BRDF LUT((NdotV,roughness) → scale/bias)
 *
 * 三张纹理以 bindless 句柄形式写入 SkyInfo.env_tex(场景全局环境数据),
 * 由材质 "LitIBL"(ShaderLibrary/material/lit_ibl.material.toml)的
 * indirect_ibl ambient 光照模块采样。场内物体是不同金属度/粗糙度的
 * PBR 材质,便于对比 IBL 在不同表面上的表现。
 */
class IBLEnvironmentApp : public WorkObject
{
private:

    struct MeshEntry
    {
        Geometry* geometry = nullptr;
        PrimitiveAsset asset{};

        ~MeshEntry()
        {
            delete geometry;
        }
    };

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

#ifdef DRAW_SKY_SPHERE
    Entity* sky_entity = nullptr;
    std::shared_ptr<EnvironmentSystem> environment_system;
    Geometry* sky_geometry = nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};
#endif//DRAW_SKY_SPHERE

#ifdef DRAW_GIZMO
    std::shared_ptr<SunDirectionControlSystem> sun_gizmo_system;
#endif//DRAW_GIZMO

    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    graph::mtl::MaterialRecipe mesh_recipe{};
    std::vector<MaterialDataAccessor> mesh_rows;           ///<每个物体一行 PBR 参数
    VertexDataManager* mesh_vdm = nullptr;

    MeshEntry* floor_mesh = nullptr;

    Texture2D* base_texture = nullptr;
    Texture2D* normal_texture = nullptr;
    Texture2D* roughness_texture = nullptr;
    Sampler* sampler = nullptr;

    TextureCube* sky_cube_texture = nullptr;                ///<天空背景环境贴图

    TextureCube* env_irradiance  = nullptr;                 ///<IBL 漫反射辐照度
    TextureCube* env_prefilter   = nullptr;                 ///<IBL 镜面预滤波
    Texture2D*   env_brdf_lut    = nullptr;                 ///<BRDF LUT

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitEnvironmentControl()
    {
        if (!ecs_context)
            return false;

    #ifdef DRAW_SKY_SPHERE
        environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (!environment_system)
            return false;

        if (auto* sky = environment_system->EditSkyInfo())
        {
            sky->sun_direction = math::Vector4f(0.2f, 0.7f, 0.68f, 0.0f);
        }
        environment_system->MarkSkyDirty();
            #endif//DRAW_SKY_SPHERE

    #ifdef DRAW_GIZMO
        sun_gizmo_system = ecs_context->GetSystem<SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            sun_gizmo_system = ecs_context->RegisterTickSystem<SunDirectionControlSystem>();

        if (!sun_gizmo_system)
            return false;

        sun_gizmo_system->SetEnvironmentSystem(environment_system.get());
        sun_gizmo_system->SetGizmoPosition(math::Vector3f(0.0f, 0.0f, 0.0f));
    #endif//DRAW_GIZMO

        return true;
    }

#ifdef DRAW_SKY_SPHERE
    bool InitSkySphereResource()
    {
        if (!ecs_context)
            return false;

        auto* geometry_manager = GetManager<GeometryManager>();
        auto* texture_manager  = GetManager<TextureManager>();
        auto* device = GetDevice();
        if (!geometry_manager || !texture_manager || !device)
            return false;

        // ---- 天空背景:sky_34 全景 cubemap ----
        sky_cube_texture = texture_manager->LoadTextureCube(
            OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k.TexCube"),
            false);

        if (!sky_cube_texture)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateSkyCubeGeometryVertexFormat());
        if (!pc)
            return false;

        HexSphereCreateInfo hsci;
        hsci.subdivisions = 3;
        hsci.radius = 256.0f;

        sky_geometry = CreateHexSphere(pc.get(), &hsci);
        if (!sky_geometry)
            return false;

        geometry_manager->Add(sky_geometry);

        sky_recipe.recipe_name = "IBLEnvironment.Sky";
        sky_recipe.mtl_def_id = "SkyCube";
        sky_recipe.render_state_overrides.pipeline_config = mtl::MakeSkyConfig();
        sky_asset = PrimitiveAsset(sky_geometry, &sky_recipe, PrimitiveType::Triangles);

        return true;
    }
#endif//DRAW_SKY_SPHERE

    bool InitMaterial()
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();
        if (!texture_manager || !sampler_manager)
            return false;

        base_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        if (!base_texture)
            return false;

        normal_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        if (!normal_texture)
            return false;

        roughness_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);
        if (!roughness_texture)
            return false;

        // ---- IBL 资源(TexConv /cube /ibl 产物 + BRDF LUT) ----
        env_irradiance = texture_manager->LoadTextureCube(
            OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube"),
            false);
        if (!env_irradiance)
            return false;

        env_prefilter = texture_manager->LoadTextureCube(
            OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube"),
            false);
        if (!env_prefilter)
            return false;

        env_brdf_lut = texture_manager->LoadTexture2D(OS_TEXT("res/image/brdflut.Tex2D"), false);
        if (!env_brdf_lut)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        // LitIBL 材质:与 Lit 相同的 PBR 表面,ambient 段换成 IBL
        mesh_recipe.recipe_name = "IBLEnvironment.Lit";
        mesh_recipe.mtl_def_id = "LitIBL";
        mesh_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();

        return true;
    }

    /// 把 IBL 纹理注册进 bindless,句柄写入 SkyInfo.env_tex(场景全局)
    bool InitIBLBinding()
    {
        if (!environment_system)
            return false;

        auto* gc  = GetGraphicsContext();
        auto* btm = gc ? gc->GetBindlessTextureManager() : nullptr;

        if (!btm)
            return false;

        const uint32_t irr_handle = btm->RegisterTexture(env_irradiance);
        const uint32_t pre_handle = btm->RegisterTexture(env_prefilter);
        const uint32_t lut_handle = btm->RegisterTexture(env_brdf_lut);

        if (!irr_handle || !pre_handle || !lut_handle)
            return false;

        auto* sky = environment_system->EditSkyInfo();
        if (!sky)
            return false;

        sky->env_tex = math::Vector4u(irr_handle, pre_handle, lut_handle, 0);
        environment_system->MarkSkyDirty();

        return true;
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardGeometryVertexFormat());
        if (!mesh_vdm)
            return false;

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
            return false;

        return true;
    }

    MeshEntry* CreateMeshEntry(Geometry* geometry)
    {
        if (!geometry)
            return nullptr;

        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return nullptr;

        geometry_manager->Add(geometry);

        auto mesh = std::make_unique<MeshEntry>();
        mesh->geometry = geometry;
        mesh->asset = PrimitiveAsset(geometry, &mesh_recipe, PrimitiveType::Triangles);

        MeshEntry* result = mesh.get();
        meshes.push_back(std::move(mesh));

        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        auto create_geometry = [this](auto&& creator) -> Geometry*
        {
            auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
            if (!pc)
                return nullptr;

            return creator(pc.get());
        };

        {
            auto geom = create_geometry([](GeometryCreater* pc)
            {
                return CreatePlaneSqaure(pc);
            });
            if (!geom)
                return false;

            floor_mesh = CreateMeshEntry(geom);
            if (!floor_mesh)
                return false;
        }

        {
            auto geom = create_geometry([](GeometryCreater* pc)
            {
                return CreateSphere(pc, 64);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {
            CubeCreateInfo cci;
            cci.segments_x = 2;
            cci.segments_y = 2;
            cci.segments_z = 2;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateCube(pc, &cci);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {
            TorusCreateInfo tci;
            tci.innerRadius = 1.9f;
            tci.outerRadius = 2.1f;
            tci.numberSlices = 128;
            tci.numberStacks = 16;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateTorus(pc, &tci);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {
            ArrowCreateInfo aci;
            aci.shaft_radius = 0.1f;
            aci.shaft_length = 2.0f;
            aci.head_radius = 0.3f;
            aci.head_length = 0.5f;
            aci.numberSlices = 16;
            aci.cross_section = ArrowCrossSection::Circular;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateArrow(pc, &aci);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        return true;
    }

    /// 每个物体一行独立的 PBR 参数,便于对比 IBL 在不同材质上的表现:
    ///   地板:白色粗糙电介质(以纯漫反射为主,看 irradiance)
    ///   球体:抛光金属(看 prefilter 反射)
    ///   立方体:半粗糙金属
    ///   圆环:红色光滑电介质
    ///   箭头:蓝色光滑电介质
    bool CreateMeshMaterialRows()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        const graph::ssbo::PBRSurfaceRow pbr_rows[5] =
        {
            {Color4f(1.0f, 1.0f, 1.0f, 1.0f), 0.00f, 0.90f, 1.0f, 0.5f},   // floor
            {Color4f(1.0f, 1.0f, 1.0f, 1.0f), 1.00f, 0.12f, 1.0f, 0.5f},   // sphere
            {Color4f(1.0f, 1.0f, 1.0f, 1.0f), 1.00f, 0.45f, 1.0f, 0.5f},   // cube
            {Color4f(0.9f, 0.1f, 0.1f, 1.0f), 0.00f, 0.25f, 1.0f, 0.5f},   // torus
            {Color4f(0.2f, 0.4f, 0.9f, 1.0f), 0.00f, 0.15f, 1.0f, 0.5f},   // arrow
        };

        mesh_rows.clear();

        for (const auto &row : pbr_rows)
        {
            mesh_rows.push_back(domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>());

            if (!mesh_rows.back())
                return false;

            if (!mesh_rows.back().Write(row))
                return false;

            if (!mesh_rows.back().GetGlobalSSBOBinding().IsValid())
                return false;
        }

        return mesh_rows.size() >= meshes.size();
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !floor_mesh || !sky_geometry)
            return false;

        if (!CreateMeshMaterialRows())
            return false;

    #ifdef DRAW_SKY_SPHERE
        {
            sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
            auto transform = sky_entity->AddComponent<TransformComponent>(Mobility::Movable);
            auto primitive_comp = sky_entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&sky_asset);
            primitive_comp->SetMaterialTextureResource("sky_cube", sky_cube_texture, sampler);
            primitive_comp->SetVisible(true);
        }
    #endif//DRAW_SKY_SPHERE

        {
            auto* entity = ecs_context->CreateEntity<Entity>("Floor");
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&floor_mesh->asset);
            primitive_comp->SetMaterialTextureResource("base_color", base_texture, sampler);
            primitive_comp->SetMaterialTextureResource("normal", normal_texture, sampler);
            primitive_comp->SetMaterialTextureResource("roughness", roughness_texture, sampler);
            hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource floor_struct{};
            floor_struct = mesh_rows[0].GetGlobalSSBOBinding();
            primitive_comp->SetMaterialDataResource(floor_struct);
            primitive_comp->SetVisible(true);
        }

        const size_t total = meshes.size();
        const size_t mesh_count = total > 1 ? (total - 1) : 1;
        size_t index = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();
            if (!rm || rm == floor_mesh)
                continue;

            const size_t row_index = index + 1;
            if (row_index >= mesh_rows.size())
                return false;

            auto* entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(mesh_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            transform->SetLocalPosition(pos);
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&rm->asset);
            hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct = mesh_rows[row_index].GetGlobalSSBOBinding();
            primitive_comp->SetMaterialDataResource(mesh_struct);
            primitive_comp->SetVisible(true);

            ++index;
        }

        return true;
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!InitEnvironmentControl())
            return false;

    #ifdef DRAW_SKY_SPHERE
        if (!InitSkySphereResource())
            return false;
    #endif//DRAW_SKY_SPHERE

        if (!InitIBLBinding())
            return false;

        if (!InitVDM())
            return false;

        if (!CreateGeometryMesh())
            return false;

        return InitSceneEntities();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 16.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~IBLEnvironmentApp()
    {
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.10f, 0.12f, 0.16f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitScene())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<IBLEnvironmentApp>(OS_TEXT("IBL Environment ECS"), argc, argv, 1280, 720);
}
