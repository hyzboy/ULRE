#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/SSBOSlotAllocator.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
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
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        gvf.Add(VertexSemantic::TexCoord, VF_V2F, 2, sizeof(float) * 2);
        gvf.Add(VertexSemantic::Normal, VF_V3F, 3, sizeof(float) * 3);
        return gvf;
    }
}

class TextureBlinnPhongMeshesECSApp : public WorkObject
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
    Entity* camera_entity = nullptr;

    Material* material = nullptr;
    DescriptorBindingSet* binding_set = nullptr;
    graph::DeviceBuffer* mi_ssbo = nullptr;
    VertexDataManager* mesh_vdm = nullptr;
    SSBOSlotAllocator slot_allocator;

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

        if (!bindless_texture_manager)
        {
            bindless_texture_manager = std::make_unique<BindlessTextureManager>();
            if (!bindless_texture_manager->Init(VkDevice(*device)))
                return false;

            render_context->SetBindlessTextureManager(bindless_texture_manager.get());
            material_manager->SetBindlessLayout(bindless_texture_manager->GetLayout());
        }

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles,
                                        mtl::WithCamera::With,
                                        mtl::WithLocalToWorld::With,
                                        mtl::WithSky::With);

        material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::Standard, &cfg);
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

        // Bindless registration is deferred to InitScene() after ECS systems are ready.

        return true;
    }

    bool InitBindlessTextureResources()
    {
        if (!ecs_context || !material || !base_texture || !normal_texture || !roughness_texture || !sampler)
            return false;

        auto rdbs = ecs_context->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
            return false;

        auto* render_context = GetRenderContext();
        auto* bindless_mgr = render_context ? render_context->GetBindlessTextureManager() : nullptr;
        if (!bindless_mgr)
            return false;

        if (rdbs->RegisterTexture2DResource("", base_texture, sampler, bindless_mgr) == 0)
            return false;
        if (rdbs->RegisterTexture2DResource("", normal_texture, sampler, bindless_mgr) == 0)
            return false;
        if (rdbs->RegisterTexture2DResource("", roughness_texture, sampler, bindless_mgr) == 0)
            return false;

        if (!rdbs->RegisterMaterialTexture(material, mtl::SamplerName::BaseColor, base_texture))
            return false;
        if (!rdbs->RegisterMaterialTexture(material, "TextureNormal", normal_texture))
            return false;
        if (!rdbs->RegisterMaterialTexture(material, "TextureRoughness", roughness_texture))
            return false;

        return true;
    }

    bool InitMISSBO()
    {
        if (!ecs_context || !material)
            return false;

        const uint32_t mi_data_bytes = material->GetMIDataBytes();
        if (mi_data_bytes == 0)
            return true;
        if (mi_data_bytes != sizeof(mtl::StandardMaterialInstance))
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* buffer_manager = graphics_context->GetBufferManager();
        auto* domain_manager = graphics_context->GetResourceDomainManager();
        if (!buffer_manager || !domain_manager)
            return false;

        auto rdbs = ecs_context->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
            return false;

        if (!slot_allocator.Init(1))
            return false;

        uint32_t slot_index = 0;
        if (!slot_allocator.Allocate(slot_index))
            return false;

        const uint32_t mi_count = 1;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * mi_data_bytes;

        mi_ssbo = buffer_manager->CreateSSBO("06c:PBRSurface:MIData", ssbo_size, nullptr, SharingMode::Exclusive);
        if (!mi_ssbo)
            return false;

        auto* gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return false;

        uint8_t* dst = static_cast<uint8_t*>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return false;

        memset(dst, 0, static_cast<size_t>(ssbo_size));
        mtl::StandardMaterialInstance mi_data{};
        mi_data.base_color = 0xFFFFFFFFu;
        mi_data.metallic = 0.08f;
        mi_data.roughness = 0.92f;
        mi_data.normal_scale = 0.35f;
        memcpy(dst + static_cast<VkDeviceSize>(slot_index) * mi_data_bytes, &mi_data, mi_data_bytes);

        gpu_buf->Unmap();

        binding_set = new DescriptorBindingSet(material);
        if (!binding_set)
            return false;

        bool has_struct_binding = false;
        for (const auto &req : material->GetBindingContract().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            if (!rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes))
                return false;

            const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
            if (!domain_manager->RegisterBuffer(addr, mi_ssbo, mi_count))
                return false;

            if (!binding_set->SetSSBOBinding(req.ssbo_type, req.ssbo_id, slot_index))
                return false;
        }

        std::cout << "[06c::InitMISSBO] registered PBRSurface SSBO: count=" << mi_count
                  << ", stride=" << mi_data_bytes << std::endl;
        return has_struct_binding;
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
        if (!geometry || !binding_set)
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

        Primitive* primitive = primitive_manager->CreatePrimitive(geometry, material, binding_set, nullptr);
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

            primitive_comp->SetPrimitive(rm_floor->primitive);
            primitive_comp->SetDescriptorBindingSet(binding_set);
            primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
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
            primitive_comp->SetDescriptorBindingSet(binding_set);
            primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
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

        if (!InitBindlessTextureResources())
            return false;

        if (!InitMISSBO())
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
        delete binding_set;
        SAFE_CLEAR(mi_ssbo)
        SAFE_CLEAR(mesh_vdm)
    }

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

int os_main(int argc, os_char** argv)
{
    return RunFramework<TextureBlinnPhongMeshesECSApp>(OS_TEXT("Standard Meshes ECS (Texture Set)"), argc, argv, 1280, 720);
}

