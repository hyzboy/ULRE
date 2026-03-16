#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
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

class BasicLitSunDirectionECSApp : public WorkObject
{
private:

    struct RenderMesh
    {
        Geometry* geometry = nullptr;
        Primitive* primitive = nullptr;

        ~RenderMesh()
        {
            delete primitive;
            delete geometry;
        }
    };

    ECSContext* ecs_context = nullptr;
    Entity* sky_entity = nullptr;
    Entity* camera_entity = nullptr;

    std::shared_ptr<EnvironmentSystem> environment_system;
    std::shared_ptr<SunDirectionControlSystem> sun_gizmo_system;

    Pipeline* sky_pipeline = nullptr;
    Geometry* sky_geometry = nullptr;
    MaterialInstance* sky_material_instance = nullptr;

    Material* material = nullptr;
    MaterialInstance* material_instance = nullptr;
    Pipeline* pipeline = nullptr;
    VertexDataManager* mesh_vdm = nullptr;

    RenderMesh* rm_floor = nullptr;

    Texture2D* base_texture = nullptr;
    Texture2D* normal_texture = nullptr;
    Texture2D* roughness_texture = nullptr;
    Sampler* sampler = nullptr;

    std::vector<std::unique_ptr<RenderMesh>> meshes;

private:

    bool InitEnvironmentControl()
    {
        if (!ecs_context)
            return false;

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

        sun_gizmo_system = ecs_context->GetSystem<SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            sun_gizmo_system = ecs_context->RegisterTickSystem<SunDirectionControlSystem>();

        if (!sun_gizmo_system)
            return false;

        sun_gizmo_system->SetEnvironmentSystem(environment_system.get());
        sun_gizmo_system->SetGizmoPosition(math::Vector3f(0.0f, 0.0f, 0.0f));

        return true;
    }

    bool InitSkySphereResource()
    {
        if (!ecs_context)
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* device = graphics_context->GetDevice();
        if (!material_manager || !geometry_manager || !device)
            return false;

        mtl::SkyMinimalCreateConfig cfg;
        sky_material_instance = material_manager->CreateMaterialInstance(mtl::MaterialPreset::SkyMinimal, &cfg);
        if (!sky_material_instance)
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        sky_pipeline = render_pass ? render_pass->CreatePipeline(sky_material_instance, InlinePipeline::Sky) : nullptr;
        if (!sky_pipeline)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(device, sky_material_instance->GetVIL());
        if (!pc)
            return false;

        HexSphereCreateInfo hsci;
        hsci.subdivisions = 3;
        hsci.radius = 256.0f;

        sky_geometry = CreateHexSphere(pc.get(), &hsci);
        if (!sky_geometry)
            return false;

        geometry_manager->Add(sky_geometry);
        return true;
    }

    bool InitMaterial()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* texture_manager = graphics_context->GetTextureManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!material_manager || !texture_manager || !sampler_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles,
                        mtl::WithCamera::With,
                        mtl::WithLocalToWorld::With,
                        mtl::WithSky::With);
        material = material_manager->CreateMaterial(mtl::MaterialPreset::Standard, &cfg);
        if (!material)
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

        if (!material->BindTextureSampler(DescriptorSetType::Material,
                                          mtl::SamplerName::BaseColor,
                                          base_texture,
                                          sampler))
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::Material,
                                          "TextureNormal",
                                          normal_texture,
                                          sampler))
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::Material,
                                          "TextureRoughness",
                                          roughness_texture,
                                          sampler))
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline)
            return false;

        mtl::StandardMaterialInstance mi_data{};
        mi_data.base_color = 0xFFFFFFFFu;
        mi_data.metallic = 0.08f;
        mi_data.roughness = 0.92f;
        mi_data.normal_scale = 0.35f;

        material_instance = material_manager->CreateMaterialInstance(material, (VIL*)nullptr, &mi_data);
        if (!material_instance)
            return false;

        return true;
    }

    bool InitVDM()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* buffer_manager = graphics_context->GetBufferManager();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(buffer_manager, material->GetDefaultVIL());
        if (!mesh_vdm)
            return false;

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
            return false;

        return true;
    }

    RenderMesh* CreateRenderMesh(Geometry* geometry)
    {
        if (!geometry || !material_instance)
            return nullptr;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return nullptr;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return nullptr;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!geometry_manager || !primitive_manager)
            return nullptr;

        geometry_manager->Add(geometry);

        Primitive* primitive = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);
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
        if (!ecs_context || !rm_floor || !sky_geometry || !sky_material_instance || !sky_pipeline)
            return false;

        {
            auto* render_context = GetRenderContext();
            if (!render_context)
                return false;

            auto* graphics_context = render_context->GetGraphicsContext();
            if (!graphics_context)
                return false;

            auto* primitive_manager = graphics_context->GetPrimitiveManager();
            if (!primitive_manager)
                return false;

            Primitive* sky_primitive = primitive_manager->CreatePrimitive(sky_geometry, sky_material_instance, sky_pipeline);
            if (!sky_primitive)
                return false;

            sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
            auto transform = sky_entity->AddComponent<TransformComponent>(Mobility::Movable);
            auto primitive_comp = sky_entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitive(sky_primitive);
            primitive_comp->SetVisible(true);
        }

        {
            auto* entity = ecs_context->CreateEntity<Entity>("Floor");
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitive(rm_floor->primitive);
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

        if (!InitSkySphereResource())
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
    ~BasicLitSunDirectionECSApp()
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
    return RunFramework<BasicLitSunDirectionECSApp>(OS_TEXT("Standard Sun Direction ECS"), argc, argv, 1280, 720);
}
