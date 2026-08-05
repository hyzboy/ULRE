#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
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
    GeometryVertexFormat CreateSkyMinimalGeometryVertexFormat()
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

class BasicLitSunDirectionECSApp : public WorkObject
{
private:

    struct RenderMesh
    {
        Geometry* geometry = nullptr;
        PrimitiveAsset asset{};

        ~RenderMesh()
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

    graph::mtl::MaterialRecipe mesh_recipe{};
    graph::SSBOArrayAccessor<ssbo::LitMaterialData>* mi_ssbo_accessor = nullptr;
    VertexDataManager* mesh_vdm = nullptr;

    RenderMesh* rm_floor = nullptr;

    Texture2D* base_texture = nullptr;
    Texture2D* normal_texture = nullptr;
    Texture2D* roughness_texture = nullptr;
    Sampler* sampler = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

    std::vector<std::unique_ptr<RenderMesh>> meshes;

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
        environment_system->SyncSkyUBO();
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
        auto* device = GetDevice();
        if (!geometry_manager || !device)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateSkyMinimalGeometryVertexFormat());
        if (!pc)
            return false;

        HexSphereCreateInfo hsci;
        hsci.subdivisions = 3;
        hsci.radius = 256.0f;

        sky_geometry = CreateHexSphere(pc.get(), &hsci);
        if (!sky_geometry)
            return false;

        geometry_manager->Add(sky_geometry);

        sky_recipe.recipe_name = "BasicLitSunDirection.Sky";
        sky_recipe.mtl_def_id = "SkyMinimal";
        sky_recipe.pipeline_preset = PipelinePreset::Sky;
        sky_recipe.domain = "BasicLitSunDirection";
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

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        // Bindless registration is deferred until ECS systems are ready.

        ssbo::LitMaterialData mi_data{};
        mi_data.base_color = Color4f(1.0f);
        mi_data.metallic = 0.08f;
        mi_data.roughness = 0.92f;
        mi_data.normal_scale = 0.35f;
        mesh_recipe.recipe_name = "BasicLitSunDirection.Lit";
        mesh_recipe.mtl_def_id = "Lit";
        mesh_recipe.pipeline_preset = PipelinePreset::Solid3D;
        mesh_recipe.domain = "BasicLitSunDirection";

        // Allocate SSBO first so the ID is available before UpsertRecipe.
        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<ssbo::LitMaterialData>(
            graph::mtl::SSBOType::ClearCoatSurface,
            "BasicLitSunDir:Standard:MI",
            1);
        if (!mi_ssbo_accessor)
            return false;

        graph::mtl::UpsertRecipeSSBOAssetBinding(mesh_recipe,
                                                 graph::mtl::DefaultMaterialSSBOName,
                                                 mi_ssbo_accessor->GetSSBOBinding());

        (*mi_ssbo_accessor)[0] = mi_data;
        mi_ssbo_accessor->Commit();

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

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
            return false;

        return true;
    }

    RenderMesh* CreateRenderMesh(Geometry* geometry)
    {
        if (!geometry)
            return nullptr;

        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return nullptr;

        geometry_manager->Add(geometry);

        auto mesh = std::make_unique<RenderMesh>();
        mesh->geometry = geometry;
        mesh->asset = PrimitiveAsset(geometry, &mesh_recipe, PrimitiveType::Triangles);

        RenderMesh* result = mesh.get();
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

            rm_floor = CreateRenderMesh(geom);
            if (!rm_floor)
                return false;
        }

        {
            auto geom = create_geometry([](GeometryCreater* pc)
            {
                return CreateSphere(pc, 64);
            });
            if (!geom || !CreateRenderMesh(geom))
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
            if (!geom || !CreateRenderMesh(geom))
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
            if (!geom || !CreateRenderMesh(geom))
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
            if (!geom || !CreateRenderMesh(geom))
                return false;
        }

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !rm_floor )
            return false;

    #ifdef DRAW_SKY_SPHERE
        if (!sky_geometry)
            return false;
    #endif//
        {
        #ifdef DRAW_SKY_SPHERE
            sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
            auto transform = sky_entity->AddComponent<TransformComponent>(Mobility::Movable);
            auto primitive_comp = sky_entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&sky_asset);
            primitive_comp->SetVisible(true);
        #endif//DRAW_SKY_SPHERE
        }

        {
            auto* entity = ecs_context->CreateEntity<Entity>("Floor");
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&rm_floor->asset);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, base_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal, normal_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Roughness, roughness_texture, sampler);
            hgl::ecs::PrimitiveComponent::MaterialSSBONamedAuthoringResource floor_struct{};
            floor_struct.ssbo_name = graph::mtl::DefaultMaterialSSBOName;
            floor_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
            floor_struct.ssbo_element_index = 0;
            floor_struct.use_ssbo_element_index = true;
            floor_struct.shared_across_instances = true;
            primitive_comp->SetMaterialSSBOResource(floor_struct);
            primitive_comp->SetVisible(true);
        }

        const size_t total = meshes.size();
        const size_t mesh_count = total > 1 ? (total - 1) : 1;
        size_t index = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();
            if (!rm || rm == rm_floor)
                continue;

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
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, base_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal, normal_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Roughness, roughness_texture, sampler);
            hgl::ecs::PrimitiveComponent::MaterialSSBONamedAuthoringResource mesh_struct{};
            mesh_struct.ssbo_name = graph::mtl::DefaultMaterialSSBOName;
            mesh_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
            mesh_struct.ssbo_element_index = 0;
            mesh_struct.use_ssbo_element_index = true;
            mesh_struct.shared_across_instances = true;
            primitive_comp->SetMaterialSSBOResource(mesh_struct);
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
    ~BasicLitSunDirectionECSApp()
    {
        auto* rc = GetRenderContext();
        auto* gc = rc ? rc->GetGraphicsContext() : nullptr;
        if (mi_ssbo_accessor)
        {
            delete mi_ssbo_accessor;
            mi_ssbo_accessor = nullptr;
        }

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
    return RunFramework<BasicLitSunDirectionECSApp>(OS_TEXT("Standard Sun Direction ECS"), argc, argv, 1280, 720);
}
