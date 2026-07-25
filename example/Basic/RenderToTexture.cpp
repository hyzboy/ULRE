#include<hgl/framework/WorkManager.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/SSBOSlotAllocator.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>

#include "../common/OffscreenWorldRuntime.h"

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include <memory>
#include <cstring>
#include <numbers>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        gvf.Add(VertexSemantic::Normal, VF_V3F, 3, sizeof(float) * 3);
        return gvf;
    }

    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        gvf.Add(VertexSemantic::TexCoord, VF_V2F, 2, sizeof(float) * 2);
        gvf.Add(VertexSemantic::Normal, VF_V3F, 3, sizeof(float) * 3);
        return gvf;
    }

    void LogTextureInfo(const char *tag, Texture2D *tex)
    {
        if (!tex)
        {
            GLogInfo("[RenderToTexture] %s tex=null", tag);
            return;
        }

        GLogInfo("[RenderToTexture] %s tex=%p image=%p view=%p layout=%u",
                tag,
                (void *)tex,
                (void *)tex->GetImage(),
                (void *)tex->GetVulkanImageView(),
                (uint32_t)tex->GetImageLayout());
    }

    bool LogStageFail(const char *stage, const char *reason)
    {
        GLogError("[RenderToTexture][%s] %s", stage, reason);
        return false;
    }

    void LogStage(const char *stage, const char *message)
    {
        GLogInfo("[RenderToTexture][%s] %s", stage, message);
    }
}

class OffscreenPass
{
private:
    hgl::example::OffscreenWorldRuntime runtime;

    RenderContext *render_context = nullptr;

    Material *mtl = nullptr;
    DescriptorBindingSet *binding_set = nullptr;
    Geometry *geometry = nullptr;
    Primitive *primitive = nullptr;
    graph::DeviceBuffer *mi_ssbo = nullptr;
    SSBOSlotAllocator slot_allocator;
    Color4f sphere_color_data{};


    bool InitMISSBO(ECSContext *world)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::InitMISSBO] begin world=%p mtl=%p binding_set=%p",
                 (void *)world, (void *)mtl, (void *)binding_set);
        if (!world || !mtl)
            return LogStageFail("OffscreenPass::InitMISSBO", "invalid input pointers");

        auto *gc = world->GetGraphicsContext();
        if (!gc && render_context)
            gc = render_context->GetGraphicsContext();

        if (!gc)
        {
            GLogError("[RenderToTexture][OffscreenPass::InitMISSBO] world_gc=%p render_context=%p rc_gc=%p",
                      (void *)(world ? world->GetGraphicsContext() : nullptr),
                      (void *)render_context,
                      (void *)(render_context ? render_context->GetGraphicsContext() : nullptr));
        }
        if (!gc)
            return LogStageFail("OffscreenPass::InitMISSBO", "graphics context is null");

        auto *buffer_manager = gc->GetBufferManager();
        auto *domain_manager = gc->GetResourceDomainManager();
        if (!buffer_manager || !domain_manager)
            return LogStageFail("OffscreenPass::InitMISSBO", "buffer/domain manager is null");

        auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
            return LogStageFail("OffscreenPass::InitMISSBO", "RenderDescriptorBindingSystem missing");

        const uint32_t mi_data_bytes = mtl->GetMIDataBytes();
        if (mi_data_bytes == 0)
        {
            LogStage("OffscreenPass::InitMISSBO", "mi_data_bytes is zero, skip SSBO");
            return true;
        }

        if (!slot_allocator.Init(1))
            return LogStageFail("OffscreenPass::InitMISSBO", "slot allocator init failed");

        uint32_t slot_index = 0;
        if (!slot_allocator.Allocate(slot_index))
            return LogStageFail("OffscreenPass::InitMISSBO", "slot allocator allocate failed");

        const uint32_t mi_count = 1;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * mi_data_bytes;
        GLogInfo("[RenderToTexture][OffscreenPass::InitMISSBO] slot=%u mi_bytes=%u mi_count=%u ssbo_size=%llu",
                 slot_index, mi_data_bytes, mi_count, static_cast<unsigned long long>(ssbo_size));

        mi_ssbo = buffer_manager->CreateSSBO("RenderToTexture:OffscreenPass:MIData", ssbo_size, nullptr, SharingMode::Exclusive);
        if (!mi_ssbo)
            return LogStageFail("OffscreenPass::InitMISSBO", "CreateSSBO failed");

        auto *gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return LogStageFail("OffscreenPass::InitMISSBO", "GetGPUBuffer failed");

        auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return LogStageFail("OffscreenPass::InitMISSBO", "SSBO map failed");

        memset(dst, 0, static_cast<size_t>(ssbo_size));
        memcpy(dst + static_cast<VkDeviceSize>(slot_index) * mi_data_bytes, &sphere_color_data, mi_data_bytes);
        gpu_buf->Unmap();

        binding_set = new DescriptorBindingSet(mtl);
        if (!binding_set)
            return LogStageFail("OffscreenPass::InitMISSBO", "binding set allocation failed");

        bool has_struct_binding = false;
        for (const auto &req : mtl->GetBindingContract().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            if (!rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes))
                return LogStageFail("OffscreenPass::InitMISSBO", "RegisterMaterialStructLayout failed");

            const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
            if (!domain_manager->RegisterBuffer(addr, mi_ssbo, mi_count))
                return LogStageFail("OffscreenPass::InitMISSBO", "RegisterBuffer failed");

            if (!binding_set->SetSSBOBinding(req.ssbo_type, req.ssbo_id, slot_index))
                return LogStageFail("OffscreenPass::InitMISSBO", "SetSSBOBinding failed");
        }

        if (!has_struct_binding)
            return LogStageFail("OffscreenPass::InitMISSBO", "MaterialInstance contract binding not found");

        LogStage("OffscreenPass::InitMISSBO", "success");
        return has_struct_binding;
    }
public:
    ~OffscreenPass()
    {
        GraphicsContext *gc = render_context ? render_context->GetGraphicsContext() : nullptr;
        if (!gc)
        {
            if (auto *w = runtime.GetWorld())
                gc = w->GetGraphicsContext();
        }

        if (gc)
        {
            if (primitive)
            {
                if (auto *pm = gc->GetPrimitiveManager())
                    pm->Release(primitive);
            }

            if (geometry)
            {
                if (auto *gm = gc->GetGeometryManager())
                    gm->Release(geometry);
            }

            if (auto *mm = gc->GetMaterialManager())
            {
            if (mtl)
                mm->Destroy(mtl);
            }
        }

        primitive = nullptr;
        geometry = nullptr;
        delete binding_set;
        binding_set = nullptr;
        mi_ssbo = nullptr;
        mtl = nullptr;
    }

    Texture2D *GetColorTexture() const
    {
        return runtime.GetColorTexture(0);
    }

    bool Init(WorkObject *owner, const uint32_t width, const uint32_t height)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::Init] begin owner=%p size=%ux%u",
                 (void *)owner, width, height);
        hgl::example::OffscreenWorldConfig cfg;
        cfg.width = width;
        cfg.height = height;
        cfg.world_name = "RenderToTexture_Offscreen";
        cfg.resource_prefix = "RenderToTexture:OffscreenRT";
        cfg.register_input_system = false;

        if (!runtime.Init(owner, cfg))
            return LogStageFail("OffscreenPass::Init", "runtime.Init failed");

        LogTextureInfo("offscreen_rt_color0_init", runtime.GetColorTexture(0));
        render_context = owner->GetRenderContext();
        GLogInfo("[RenderToTexture][OffscreenPass::Init] success world=%p rt=%p",
                 (void *)runtime.GetWorld(), (void *)runtime.GetRenderTarget());
        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::BuildSphere] begin owner=%p world=%p rt=%p",
                 (void *)owner, (void *)runtime.GetWorld(), (void *)runtime.GetRenderTarget());
        if (!owner || !runtime.GetWorld() || !runtime.GetRenderTarget())
            return LogStageFail("OffscreenPass::BuildSphere", "owner/world/render target missing");

        RenderContext *rc = owner->GetRenderContext();
        if (!rc)
            return LogStageFail("OffscreenPass::BuildSphere", "render context is null");

        GraphicsContext *gc = rc->GetGraphicsContext();
        if (!gc)
            return LogStageFail("OffscreenPass::BuildSphere", "graphics context is null");

        auto *mm = gc->GetMaterialManager();
        auto *gm = gc->GetGeometryManager();
        auto *pm = gc->GetPrimitiveManager();
        auto *device = gc->GetDevice();
        if (!mm || !gm || !pm || !device)
            return LogStageFail("OffscreenPass::BuildSphere", "required managers/device missing");

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::Without);

        mtl = mm->CreateMaterial(mtl::MaterialPreset::Gizmo3D, &cfg3d);
        if (!mtl)
            return LogStageFail("OffscreenPass::BuildSphere", "CreateMaterial(Gizmo3D) failed");

        sphere_color_data = GetColor4f(COLOR::SkyBlue, 1.0f);

        if (!InitMISSBO(runtime.GetWorld()))
            return LogStageFail("OffscreenPass::BuildSphere", "InitMISSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateGizmo3DGeometryVertexFormat());
        geometry = inline_geometry::CreateSphere(pc.get(), 64);
        if (!geometry)
            return LogStageFail("OffscreenPass::BuildSphere", "CreateSphere geometry failed");

        gm->Add(geometry);

        primitive = pm->CreatePrimitive(geometry, mtl, binding_set, nullptr);
        if (!primitive)
            return LogStageFail("OffscreenPass::BuildSphere", "CreatePrimitive failed");

        auto *world = runtime.GetWorld();
        Entity *sphere = world->CreateEntity<Entity>("OffscreenSphere");
        auto transform = sphere->AddComponent<TransformComponent>(Mobility::Static);
        auto prim_comp = sphere->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(primitive);
        prim_comp->SetDescriptorBindingSet(binding_set);
        prim_comp->RequestPipeline(InlinePipeline::Solid3D);
        prim_comp->SetVisible(true);

        Entity *camera_entity = world->CreateEntity<Entity>("OffscreenCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();
        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        auto camera_system = runtime.GetCameraSystem();
        camera->camera_data = camera_system ? camera_system->GetCamera() : nullptr;
        camera->camera_info = const_cast<CameraInfo *>(camera_system ? camera_system->GetCameraInfo() : nullptr);
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        LogStage("OffscreenPass::BuildSphere", "success");
        return true;
    }

    bool RenderOnce()
    {
        LogStage("OffscreenPass::RenderOnce", "begin");
        if (!runtime.RenderOnce(GetColor4f(COLOR::DarkSlateBlue, 1.0f)))
            return LogStageFail("OffscreenPass::RenderOnce", "runtime.RenderOnce failed");

        LogStage("OffscreenPass::RenderOnce", "success");
        return true;
    }
};

class RenderToTextureApp final: public WorkObject
{
private:
    OffscreenPass *offscreen = nullptr;

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;
    Entity *cube_entity = nullptr;

    Material *cube_mtl = nullptr;
    DescriptorBindingSet *cube_binding_set = nullptr;
    graph::DeviceBuffer *cube_mi_ssbo = nullptr;
    Sampler *cube_sampler = nullptr;
    Primitive *cube_primitive = nullptr;
    SSBOSlotAllocator cube_slot_allocator;
    mtl::StandardMaterialInstance cube_mi_data{};

    Texture2D *base_tex = nullptr;
    Texture2D *fallback_albedo = nullptr;
    Texture2D *normal_tex = nullptr;
    Texture2D *roughness_tex = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

    std::shared_ptr<TransformComponent> cube_transform;
    float cube_theta = 0.0f;

private:
    bool SetupMainCamera()
    {
        LogStage("RenderToTextureApp::SetupMainCamera", "begin");
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("RenderToTextureApp::SetupMainCamera", "ECS context/camera system unavailable");

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 5.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();
        LogStage("RenderToTextureApp::SetupMainCamera", "success");
        return true;
    }

    bool CreateOffscreenRT()
    {
        LogStage("RenderToTextureApp::CreateOffscreenRT", "begin");
        offscreen = new OffscreenPass();
        if (!offscreen)
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "new OffscreenPass failed");

        if (!offscreen->Init(this, 512, 512))
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "OffscreenPass::Init failed");

        if (!offscreen->BuildSphere(this))
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "OffscreenPass::BuildSphere failed");

        // This sample's offscreen content is static. Render once at startup to
        // avoid cross-pass write/read jitter on the same texture each frame.
        if (!offscreen->RenderOnce())
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "OffscreenPass::RenderOnce failed");

        LogStage("RenderToTextureApp::CreateOffscreenRT", "success");
        return true;
    }

    bool CreateCube()
    {
        LogStage("RenderToTextureApp::CreateCube", "begin");
        auto *rc = GetRenderContext();
        if (!rc)
            return LogStageFail("RenderToTextureApp::CreateCube", "render context is null");

        auto *gc = rc->GetGraphicsContext();
        if (!gc)
            return LogStageFail("RenderToTextureApp::CreateCube", "graphics context is null");

        auto *mm = gc->GetMaterialManager();
        auto *sm = gc->GetSamplerManager();
        auto *tm = gc->GetTextureManager();
        auto *gm = gc->GetGeometryManager();
        auto *pm = gc->GetPrimitiveManager();
        auto *device = gc->GetDevice();
        if (!mm || !sm || !tm || !gm || !pm || !device)
            return LogStageFail("RenderToTextureApp::CreateCube", "required managers/device missing");

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::With);

        cube_mtl = mm->CreateMaterial(mtl::MaterialPreset::Standard, &cfg3d);
        if (!cube_mtl)
            return LogStageFail("RenderToTextureApp::CreateCube", "CreateMaterial(Standard) failed");

        cube_sampler = sm->CreateSampler();
        if (!cube_sampler)
            return LogStageFail("RenderToTextureApp::CreateCube", "CreateSampler failed");

        base_tex = offscreen ? offscreen->GetColorTexture() : nullptr;
        if (!base_tex)
        {
            LogStage("RenderToTextureApp::CreateCube", "offscreen texture unavailable, loading fallback albedo");
            fallback_albedo = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
            base_tex = fallback_albedo;
        }

        normal_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        roughness_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);

        if (!base_tex || !normal_tex || !roughness_tex)
            return LogStageFail("RenderToTextureApp::CreateCube", "required textures missing");

        if (!bindless_texture_manager)
        {
            bindless_texture_manager = std::make_unique<BindlessTextureManager>();
            if (!bindless_texture_manager->Init(VkDevice(*device)))
                return LogStageFail("RenderToTextureApp::CreateCube", "BindlessTextureManager::Init failed");

            rc->SetBindlessTextureManager(bindless_texture_manager.get());
            mm->SetBindlessLayout(bindless_texture_manager->GetLayout());
        }

        auto rdbs = ecs_context ? ecs_context->GetSystem<RenderDescriptorBindingSystem>() : nullptr;
        if (!rdbs)
            return LogStageFail("RenderToTextureApp::CreateCube", "RenderDescriptorBindingSystem missing");

        auto* bindless_mgr = rc->GetBindlessTextureManager();
        if (!bindless_mgr)
            return LogStageFail("RenderToTextureApp::CreateCube", "bindless manager missing on render context");

        const uint32_t base_tex_handle = rdbs->RegisterTexture2DResource("", base_tex, cube_sampler, bindless_mgr);
        const uint32_t normal_tex_handle = rdbs->RegisterTexture2DResource("", normal_tex, cube_sampler, bindless_mgr);
        const uint32_t roughness_tex_handle = rdbs->RegisterTexture2DResource("", roughness_tex, cube_sampler, bindless_mgr);
        GLogInfo("[RenderToTexture][RenderToTextureApp::CreateCube] bindless handles base=%u normal=%u roughness=%u",
                 base_tex_handle, normal_tex_handle, roughness_tex_handle);
        if (base_tex_handle == 0 || normal_tex_handle == 0 || roughness_tex_handle == 0)
            return LogStageFail("RenderToTextureApp::CreateCube", "RegisterTexture2DResource returned 0 handle");

        if (!rdbs->RegisterMaterialTexture(cube_mtl, mtl::SamplerName::BaseColor, base_tex))
            return LogStageFail("RenderToTextureApp::CreateCube", "RegisterMaterialTexture(BaseColor) failed");
        if (!rdbs->RegisterMaterialTexture(cube_mtl, "TextureNormal", normal_tex))
            return LogStageFail("RenderToTextureApp::CreateCube", "RegisterMaterialTexture(TextureNormal) failed");
        if (!rdbs->RegisterMaterialTexture(cube_mtl, "TextureRoughness", roughness_tex))
            return LogStageFail("RenderToTextureApp::CreateCube", "RegisterMaterialTexture(TextureRoughness) failed");

        LogTextureInfo("onscreen_bind_basecolor", base_tex);

        cube_mi_data.base_color = 0xFFFFFFFFu;
        cube_mi_data.metallic = 0.08f;
        cube_mi_data.roughness = 0.92f;
        cube_mi_data.normal_scale = 0.35f;

        if (!InitCubeMISSBO())
            return LogStageFail("RenderToTextureApp::CreateCube", "InitCubeMISSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());
        inline_geometry::CubeCreateInfo cci{};
        cci.tex_coord = true;
        cci.normal = true;

        Geometry *cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
        if (!cube_geometry)
            return LogStageFail("RenderToTextureApp::CreateCube", "CreateCube geometry failed");

        gm->Add(cube_geometry);

        cube_primitive = pm->CreatePrimitive(cube_geometry,
                                             cube_mtl,
                                             cube_binding_set,
                                             nullptr);
        if (!cube_primitive)
            return LogStageFail("RenderToTextureApp::CreateCube", "CreatePrimitive failed");

        cube_entity = ecs_context->CreateEntity<Entity>("RTTCube");
        cube_transform = cube_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto cube_prim_comp = cube_entity->AddComponent<PrimitiveComponent>();

        cube_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube_transform->SetMovable(true);

        cube_prim_comp->SetPrimitive(cube_primitive);
        cube_prim_comp->SetDescriptorBindingSet(cube_binding_set);
        cube_prim_comp->RequestPipeline(InlinePipeline::Solid3D);
        cube_prim_comp->SetVisible(true);
        LogStage("RenderToTextureApp::CreateCube", "success");
        return true;
    }
    bool InitCubeMISSBO()
    {
        GLogInfo("[RenderToTexture][RenderToTextureApp::InitCubeMISSBO] begin ecs=%p mtl=%p binding_set=%p",
                 (void *)ecs_context, (void *)cube_mtl, (void *)cube_binding_set);
        if (!ecs_context || !cube_mtl)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "invalid input pointers");

        auto *rc = GetRenderContext();
        if (!rc)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "render context is null");

        auto *gc = rc->GetGraphicsContext();
        if (!gc)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "graphics context is null");

        auto *buffer_manager = gc->GetBufferManager();
        auto *domain_manager = gc->GetResourceDomainManager();
        if (!buffer_manager || !domain_manager)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "buffer/domain manager is null");

        auto rdbs = ecs_context->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "RenderDescriptorBindingSystem missing");

        const uint32_t mi_data_bytes = cube_mtl->GetMIDataBytes();
        if (mi_data_bytes == 0)
        {
            LogStage("RenderToTextureApp::InitCubeMISSBO", "mi_data_bytes is zero, skip SSBO");
            return true;
        }

        if (!cube_slot_allocator.Init(1))
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "slot allocator init failed");

        uint32_t slot_index = 0;
        if (!cube_slot_allocator.Allocate(slot_index))
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "slot allocator allocate failed");

        const uint32_t mi_count = 1;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * mi_data_bytes;
        GLogInfo("[RenderToTexture][RenderToTextureApp::InitCubeMISSBO] slot=%u mi_bytes=%u mi_count=%u ssbo_size=%llu",
                 slot_index, mi_data_bytes, mi_count, static_cast<unsigned long long>(ssbo_size));

        cube_mi_ssbo = buffer_manager->CreateSSBO("RenderToTexture:MainScene:MIData", ssbo_size, nullptr, SharingMode::Exclusive);
        if (!cube_mi_ssbo)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "CreateSSBO failed");

        auto *gpu_buf = cube_mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "GetGPUBuffer failed");

        auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "SSBO map failed");

        memset(dst, 0, static_cast<size_t>(ssbo_size));
        memcpy(dst + static_cast<VkDeviceSize>(slot_index) * mi_data_bytes, &cube_mi_data, mi_data_bytes);
        gpu_buf->Unmap();

        cube_binding_set = new DescriptorBindingSet(cube_mtl);
        if (!cube_binding_set)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "binding set allocation failed");

        bool has_struct_binding = false;
        for (const auto &req : cube_mtl->GetBindingContract().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            if (!rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes))
                return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "RegisterMaterialStructLayout failed");

            const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
            if (!domain_manager->RegisterBuffer(addr, cube_mi_ssbo, mi_count))
                return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "RegisterBuffer failed");

            if (!cube_binding_set->SetSSBOBinding(req.ssbo_type, req.ssbo_id, slot_index))
                return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "SetSSBOBinding failed");
        }

        if (!has_struct_binding)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "MaterialInstance contract binding not found");

        LogStage("RenderToTextureApp::InitCubeMISSBO", "success");
        return has_struct_binding;
    }
public:
    ~RenderToTextureApp() override
    {
        SAFE_CLEAR(offscreen)

        if (auto *rc = GetRenderContext())
        {
            auto *gc = rc->GetGraphicsContext();

            if (cube_primitive)
            {
                if (auto *pm = gc ? gc->GetPrimitiveManager() : nullptr)
                    pm->Release(cube_primitive);
            }

            if (auto *mm = gc ? gc->GetMaterialManager() : nullptr)
            {
                if (cube_mtl)
                    mm->Destroy(cube_mtl);
            }

            if (cube_sampler)
            {
                if (auto *sm = gc ? gc->GetSamplerManager() : nullptr)
                    sm->Release(cube_sampler);
            }
        }

        cube_primitive = nullptr;
        delete cube_binding_set;
        cube_binding_set = nullptr;
        cube_mi_ssbo = nullptr;
        cube_mtl = nullptr;
        cube_sampler = nullptr;
        base_tex = nullptr;
        fallback_albedo = nullptr;
        normal_tex = nullptr;
        roughness_tex = nullptr;
    }

    bool Init() override
    {
        LogStage("RenderToTextureApp::Init", "begin");
        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("RenderToTextureApp::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("RenderToTexture:MainScene");

        auto environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (environment_system)
        {
            environment_system->EditSkyInfo();
            environment_system->SyncSkyUBO();
        }

        if (!CreateOffscreenRT())
            return LogStageFail("RenderToTextureApp::Init", "CreateOffscreenRT failed");

        if (!CreateCube())
            return LogStageFail("RenderToTextureApp::Init", "CreateCube failed");

        if (!SetupMainCamera())
            return LogStageFail("RenderToTextureApp::Init", "SetupMainCamera failed");

        LogStage("RenderToTextureApp::Init", "success");
        return true;
    }

    void Render(double delta_time) override
    {
        if (cube_transform)
        {
            cube_theta += static_cast<float>(delta_time) * 0.8f;
            cube_theta = fmodf(cube_theta, 2.0f * std::numbers::pi_v<float>);

            const glm::quat rot_z = glm::angleAxis(cube_theta, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::quat rot_x = glm::angleAxis(cube_theta * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
            cube_transform->SetLocalRotation(rot_z * rot_x);
        }

        WorkObject::Render(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<RenderToTextureApp>(OS_TEXT("Render To Texture (ECS)"), argc, argv, 1280, 720);
}
