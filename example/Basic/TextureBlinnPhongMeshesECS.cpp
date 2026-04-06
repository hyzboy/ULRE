#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>

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

#include<vector>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class TextureBlinnPhongMeshesECSApp : public WorkObject
{
private:

    struct RenderMesh
    {
        Geometry* geometry = nullptr;
        Primitive* primitive = nullptr;

        ~RenderMesh()
        {
        }
    };

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    ShaderProgram* material = nullptr;
    MaterialInstance* material_instance = nullptr;
    SemanticMaterialId standard_semantic_id = 0;
    VertexDataManager* mesh_vdm = nullptr;

    RenderMesh* rm_floor = nullptr;

    Texture2D* base_texture = nullptr;
    Texture2D* normal_texture = nullptr;
    Sampler* sampler = nullptr;

    std::vector<std::unique_ptr<RenderMesh>> meshes;

    Entity* sky_entity = nullptr;

private:

    bool InitMaterial()
    {

        auto* texture_manager = GetTextureManager();
        auto* sampler_manager = GetSamplerManager();
        if (!texture_manager || !sampler_manager)
            return false;

        static const mtl::MaterialAssetRecord kStandardCfg {
            .id             = "blinnphong_standard",
            .preset         = mtl::MaterialPreset::Standard,
            .sky            = true,
            .sky_ambient    = mtl::SkyLightAmbientModel::FakeAtmosphere,
            .lighting       = mtl::LightingModel::BlinnPhong,
            .pipeline       = GraphicsPipelinePreset::Solid3D,
            .textures       = {
                {mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::None, "res/image/Brickwall/Albedo.Tex2D"},
                {mtl::SamplerSlot::Normal,    mtl::TextureSourceMode::None, "res/image/Brickwall/Normal.Tex2D"},
            },
        };
        mtl::StandardMaterialInstance mi_data{};
        mi_data.base_color = 0xFFFFFFFFu;
        mi_data.metallic = 0.08f;
        mi_data.roughness = 0.92f;
        mi_data.normal_scale = 0.35f;

        standard_semantic_id = RegisterSemanticMaterial(kStandardCfg);
        if (standard_semantic_id == 0)
            return false;

        material_instance = AcquireMI(kStandardCfg, &mi_data, sizeof(mi_data));
        if (!material_instance)
            return false;

        return true;
    }

    bool InitVDM()
    {

        auto* buffer_manager = GetBufferManager();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(buffer_manager, material_instance->GetVIL());
        if (!mesh_vdm)
            return false;

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
            return false;

        return true;
    }

    RenderMesh* CreateRenderMesh(Geometry* geometry)
    {
        if (!geometry || standard_semantic_id == 0)
            return nullptr;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return nullptr;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return nullptr;

        auto* primitive_manager = GetPrimitiveManager();
        if (!primitive_manager)
            return nullptr;

        GraphicsGeometryFactory geometry_factory(graphics_context);
        Primitive* primitive = geometry_factory.CreatePrimitive(geometry, standard_semantic_id);
        if (!primitive)
            return nullptr;

        auto mesh = std::make_unique<RenderMesh>();
        mesh->geometry = geometry;
        mesh->primitive = primitive;

        RenderMesh* result = mesh.get();
        meshes.push_back(std::move(mesh));

        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        auto create_geometry = [this, &geometry_factory](auto&& creator) -> Geometry*
        {
            auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
            if (!pc)
                return nullptr;

            auto* geometry = creator(pc.get());
            if (!geometry)
                return nullptr;

            return geometry_factory.RegisterGeometry(geometry);
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
                    DomeCreateInfo dci;
                    dci.number_slices = 64;
                    return CreateDome(pc, &dci);
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
            TubeCreateInfo tci;
            tci.length = 1.25f * 2.0f;
            tci.inner_radius = 0.8f;
            tci.outer_radius = 1.25f;
            tci.segments = 64;
            tci.generate_caps = true;

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateTube(pc, &tci);
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

            primitive_comp->SetPrimitive(rm_floor->primitive);
            primitive_comp->SetSemanticMaterial(standard_semantic_id);
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

            primitive_comp->SetPrimitive(rm->primitive);
            primitive_comp->SetSemanticMaterial(standard_semantic_id);
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

    bool InitSkySphere()
    {

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        static const mtl::MaterialAssetRecord kSkyCfg {
            .id       = "blinnphong_sky",
            .preset   = mtl::MaterialPreset::SkyMinimal,
            .l2w      = false,
            .sky      = true,
            .pipeline = GraphicsPipelinePreset::Sky,
        };
        const SemanticMaterialId sky_semantic_id = RegisterSemanticMaterial(kSkyCfg);
        if (sky_semantic_id == 0)
            return false;

        Primitive* ri = GraphicsGeometryFactory::CreatePrimitive(graphics_context,
                                                                 sky_semantic_id,
                                                                 [](GeometryCreater* pc)
                                                                 {
                                                                     using namespace inline_geometry;
                                                                     HexSphereCreateInfo hsci;
                                                                     hsci.subdivisions = 3;
                                                                     hsci.radius = 256;
                                                                     return CreateHexSphere(pc, &hsci);
                                                                 });
        if (!ri)
            return false;

        sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
        auto transform = sky_entity->AddComponent<TransformComponent>(Mobility::Movable);
        auto prim_comp = sky_entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(ri);
        prim_comp->SetSemanticMaterial(sky_semantic_id);
        prim_comp->SetVisible(true);

        return true;
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
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.18f, 0.18f, 0.20f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitScene())
            return false;

        {
            auto env = ecs_context->GetSystem<EnvironmentSystem>();
            if (!env)
                env = ecs_context->RegisterRenderSystem<EnvironmentSystem>();
            if (env)
            {
                env->EditSkyInfo();
                env->SyncSkyUBO();
            }
        }

        if (!InitSkySphere())
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

