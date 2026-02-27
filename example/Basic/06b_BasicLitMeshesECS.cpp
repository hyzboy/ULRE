#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<vector>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class BasicLitMeshesECSApp : public WorkObject
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

    ECSContext* ecs_world = nullptr;
    Entity* camera_entity = nullptr;

    Material* material = nullptr;
    Pipeline* pipeline = nullptr;

    Texture2D* base_texture = nullptr;
    Sampler* sampler = nullptr;

    std::vector<MaterialInstance*> material_instances;
    std::vector<std::unique_ptr<RenderMesh>> meshes;

private:

    bool EnsureCameraSystem()
    {
        if (!ecs_world)
            return false;

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
        auto* device = graphics_context->GetDevice();
        if (!material_manager || !texture_manager || !sampler_manager || !device)
            return false;

        mtl::BasicLitMaterialCreateConfig cfg(false);
        mtl::MaterialCreateInfo* mci = mtl::CreateBasicLit(device->GetDevAttr(), &cfg);
        if (!mci)
            return false;

        material = material_manager->CreateMaterial("BasicLitMeshes", mci);
        if (!material)
            return false;

        base_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"), true);
        if (!base_texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::PerMaterial,
                                          mtl::SamplerName::BaseColor,
                                          base_texture,
                                          sampler))
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::PerMaterial,
                                          "TextureNormal",
                                          base_texture,
                                          sampler))
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::PerMaterial,
                                          "TextureRoughness",
                                          base_texture,
                                          sampler))
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline)
            return false;

        const COLOR colors[] = {
            COLOR::BlenderAxisRed,
            COLOR::BlenderAxisGreen,
            COLOR::BlenderAxisBlue,
            COLOR::BananaYellow,
            COLOR::Coral,
            COLOR::Lavender,
        };

        for (auto c : colors)
        {
            mtl::BasicLitMaterialInstance mi_data{};
            mi_data.base_color = GetRGBA(c);
            mi_data.metallic = 0.15f;
            mi_data.roughness = 0.85f;
            mi_data.fresnel = 0.04f;
            mi_data.ibl_intensity = 0.0f;

            auto* mi = material_manager->CreateMaterialInstance(material, (VIL*)nullptr, &mi_data);
            if (!mi)
                return false;

            material_instances.push_back(mi);
        }

        return true;
    }

    bool AddMeshEntity(const char* name, Geometry* geometry, MaterialInstance* mi, const glm::vec3& pos)
    {
        if (!geometry || !mi)
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!geometry_manager || !primitive_manager)
            return false;

        geometry_manager->Add(geometry);

        Primitive* primitive = primitive_manager->CreatePrimitive(geometry, mi, pipeline);
        if (!primitive)
            return false;

        auto mesh = std::make_unique<RenderMesh>();
        mesh->geometry = geometry;
        mesh->primitive = primitive;
        meshes.push_back(std::move(mesh));

        auto* entity = ecs_world->CreateEntity<Entity>(name);
        auto transform = entity->AddComponent<TransformComponent>();
        auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(pos);
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        primitive_comp->SetPrimitive(primitive);
        primitive_comp->SetVisible(true);

        return true;
    }

    bool InitScene()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        if (!EnsureCameraSystem())
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        using namespace inline_geometry;

        auto pc1 = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());
        CubeCreateInfo cube_ci;
        cube_ci.normal = true;
        cube_ci.tex_coord = true;
        if (!AddMeshEntity("BasicLitCube", CreateCube(pc1.get(), &cube_ci), material_instances[0], glm::vec3(-4.0f, -1.2f, 0.0f)))
            return false;

        auto pc2 = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());
        if (!AddMeshEntity("BasicLitSphere", CreateSphere(pc2.get(), 32), material_instances[1], glm::vec3(-1.5f, -1.2f, 0.0f)))
            return false;

        auto pc3 = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());
        CylinderCreateInfo cyl_ci;
        cyl_ci.halfExtend = 1.0f;
        cyl_ci.radius = 0.8f;
        cyl_ci.numberSlices = 24;
        if (!AddMeshEntity("BasicLitCylinder", CreateCylinder(pc3.get(), &cyl_ci), material_instances[2], glm::vec3(1.0f, -1.2f, 0.0f)))
            return false;

        auto pc4 = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());
        TorusCreateInfo torus_ci;
        torus_ci.innerRadius = 0.5f;
        torus_ci.outerRadius = 1.0f;
        torus_ci.numberSlices = 48;
        torus_ci.numberStacks = 16;
        if (!AddMeshEntity("BasicLitTorus", CreateTorus(pc4.get(), &torus_ci), material_instances[3], glm::vec3(3.6f, -1.2f, 0.0f)))
            return false;

        return true;
    }

    bool InitCamera()
    {
        if (!EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, -1.2f, 0.0f);
        camera->distance = 14.0f;
        camera->yaw = 45.0f;
        camera->pitch = -18.0f;
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
        SetClearColor(Color4f(0.18f, 0.18f, 0.20f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitScene())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int, os_char**)
{
    return RunFramework<BasicLitMeshesECSApp>(OS_TEXT("BasicLit Meshes ECS"), 1280, 720);
}
