#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<vector>
#include<memory>
#include<cstring>
#include<string>

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

class TextureBlinnPhongMeshesECSApp : public WorkObject
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

    bool InitMaterial()
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();
        if (!texture_manager || !sampler_manager)
            return false;
        mesh_recipe.recipe_name = "06c.TextureBlinnPhong.Lit";
        mesh_recipe.mtl_def_id = "Lit";
        mesh_recipe.pipeline_preset = PipelinePreset::Solid3D;
        mesh_recipe.domain = "06c.TextureBlinnPhong";
        graph::mtl::UpsertRecipeSSBOAssetBinding(mesh_recipe,
                                                 graph::mtl::DefaultMaterialDataSlotName,
                                                 mi_ssbo_accessor->GetSSBOBinding());

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

        // Bindless registration is deferred to InitScene() after ECS systems are ready.

        return true;
    }

    bool InitMISSBO()
    {
        auto* domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        ssbo::LitMaterialData mi_data{};
        mi_data.base_color  = Color4f(1.0f);
        mi_data.metallic    = 0.08f;
        mi_data.roughness   = 0.92f;
        mi_data.normal_scale = 0.35f;

        mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<ssbo::LitMaterialData>(
            graph::mtl::SSBOType::ClearCoatSurface,
            "06c:ClearCoatSurface:MIData",
            1);
        if (!mi_ssbo_accessor)
            return false;

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
            auto geom = create_geometry([](GeometryCreater* pc)
            {
                return CreateDome(pc, 64);
            });
            if (!geom || !CreateRenderMesh(geom))
                return false;
        }

        {
            ConeCreateInfo cci;
            cci.radius = 1;
            cci.halfExtend = 1;
            cci.numberSlices = 64;
            cci.numberStacks = 4;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateCone(pc, &cci);
            });
            if (!geom || !CreateRenderMesh(geom))
                return false;
        }

        {
            CylinderCreateInfo cci;
            cci.halfExtend = 1.25f;
            cci.numberSlices = 16;
            cci.radius = 1.25f;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateCylinder(pc, &cci);
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
            HollowCylinderCreateInfo hcci;
            hcci.halfExtend = 1.25f;
            hcci.innerRadius = 0.8f;
            hcci.outerRadius = 1.25f;
            hcci.numberSlices = 64;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateHollowCylinder(pc, &hcci);
            });
            if (!geom || !CreateRenderMesh(geom))
                return false;
        }

        {
            HexSphereCreateInfo hsci;
            hsci.subdivisions = 3;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateHexSphere(pc, &hsci);
            });
            if (!geom || !CreateRenderMesh(geom))
                return false;
        }

        {
            CapsuleCreateInfo cci;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateCapsule(pc, &cci);
            });
            if (!geom || !CreateRenderMesh(geom))
                return false;
        }

        {
            TaperedCapsuleCreateInfo tcci;
            tcci.topRadius = 0.1f;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateTaperedCapsule(pc, &tcci);
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
            FrustumCreateInfo fci;
            fci.bottom_radius = 1.0f;
            fci.top_radius = 0.5f;
            fci.height = 2.0f;
            fci.numberSlices = 32;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateFrustum(pc, &fci);
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

        {
            PipeElbowCreateInfo peci;
            peci.inner_radius = 0.3f;
            peci.outer_radius = 0.5f;
            peci.bend_angle = 90.0f;
            peci.bend_radius = 1.0f;
            peci.pipe_segments = 16;
            peci.bend_segments = 16;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreatePipeElbow(pc, &peci);
            });
            if (!geom || !CreateRenderMesh(geom))
                return false;
        }

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !rm_floor)
            return false;

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
            hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource floor_struct{};
            floor_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            floor_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
            floor_struct.data_index = 0;
            floor_struct.use_data_index = false;
            floor_struct.shared_across_instances = false;
            primitive_comp->SetMaterialDataSlotResource(floor_struct);
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
            hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource mesh_struct{};
            mesh_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            mesh_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
            mesh_struct.data_index = 0;
            mesh_struct.use_data_index = false;
            mesh_struct.shared_across_instances = false;
            primitive_comp->SetMaterialDataSlotResource(mesh_struct);
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
        camera->distance = 14.0f;
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
    ~TextureBlinnPhongMeshesECSApp()
    {
        SAFE_CLEAR(mi_ssbo_accessor)
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.18f, 0.18f, 0.20f, 1.0f));

        if (!InitMISSBO())
            return false;

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
    return RunFramework<TextureBlinnPhongMeshesECSApp>(OS_TEXT("Standard Meshes ECS (Texture Set)"), argc, argv, 1280, 720);
}
