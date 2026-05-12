#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/module/TextureDomainRegistry.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKDomainResourceBinding.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKFormat.h>
#include<cstdint>
#include<unordered_map>

namespace hgl::ecs
{
    static graph::ResourceDomain *ResolveDomainForMaterial(graph::GraphicsContext *gc,
                                                           graph::ShaderMaterialProgram *material,
                                                           uint32_t domain_id)
    {
        if (!material)
            return nullptr;

        auto *rdm = gc ? gc->GetResourceDomainManager() : nullptr;
        if (!rdm)
            return nullptr;

        const auto schema = material->GetShaderDataSchema();

        if (auto *domain = rdm->Get(schema, domain_id))
            return domain;

        graph::ResourceDomainCreateInfo ci;
        ci.schema = schema;
        ci.domain_id = domain_id;
        ci.initial_capacity = 256;
        return rdm->Create(ci);
    }

    // Static member initialization
    graph::Primitive* QuadResourcePrepareSystem::shared_primitive = nullptr;
    graph::MaterialBindingInstance* QuadResourcePrepareSystem::shared_material_instance = nullptr;
    graph::RenderTargetFormat* QuadResourcePrepareSystem::shared_render_pass = nullptr;
    graph::Sampler* QuadResourcePrepareSystem::shared_sampler = nullptr;

    static graph::GraphicsPipelinePreset g_default_quad_inline_pipeline = graph::GraphicsPipelinePreset::Solid3D;
    static std::unordered_map<const ECSContext*, graph::GraphicsPipelinePreset> g_world_quad_inline_pipeline;
    static std::unordered_map<const ECSContext*, graph::TextureChannelHint> g_world_quad_channel_hint;
    static std::unordered_map<const ECSContext*, bool> g_world_billboard_fixed_size;

    static graph::RenderAlphaMode GraphicsPipelinePresetToBlendMode(graph::GraphicsPipelinePreset pipeline)
    {
        switch (pipeline)
        {
        case graph::GraphicsPipelinePreset::Masked3D:           return graph::RenderAlphaMode::Masked;
        case graph::GraphicsPipelinePreset::Dither3D:           return graph::RenderAlphaMode::Dither;
        case graph::GraphicsPipelinePreset::AlphaToCoverage3D:  return graph::RenderAlphaMode::AlphaToCoverage;
        case graph::GraphicsPipelinePreset::Alpha3D:            return graph::RenderAlphaMode::Transparent;
        default:                                                return graph::RenderAlphaMode::Opaque;
        }
    }

    void QuadResourcePrepareSystem::SetPresetForWorld(const ECSContext* world,
                                                      graph::GraphicsPipelinePreset preset)
    {
        if (!world)
            return;

        g_world_quad_inline_pipeline[world] = preset;
    }

    graph::GraphicsPipelinePreset QuadResourcePrepareSystem::GetPresetForWorld(const ECSContext* world)
    {
        if (world)
        {
            auto it = g_world_quad_inline_pipeline.find(world);
            if (it != g_world_quad_inline_pipeline.end())
                return it->second;
        }

        return g_default_quad_inline_pipeline;
    }

    void QuadResourcePrepareSystem::SetPipelineForWorld(const ECSContext* world,
                                                        graph::GraphicsPipelinePreset pipeline)
    {
        SetPresetForWorld(world, pipeline);
    }

    graph::GraphicsPipelinePreset QuadResourcePrepareSystem::GetPipelineForWorld(const ECSContext* world)
    {
        return GetPresetForWorld(world);
    }

    graph::RenderAlphaMode QuadResourcePrepareSystem::GetBlendModeForWorld(const ECSContext* world)
    {
        return GraphicsPipelinePresetToBlendMode(GetPresetForWorld(world));
    }

    void QuadResourcePrepareSystem::SetChannelHintForWorld(const ECSContext* world,
                                                           graph::TextureChannelHint hint)
    {
        if (!world)
            return;

        g_world_quad_channel_hint[world] = hint;
    }

    graph::TextureChannelHint QuadResourcePrepareSystem::GetChannelHintForWorld(const ECSContext* world)
    {
        if (world)
        {
            auto it = g_world_quad_channel_hint.find(world);
            if (it != g_world_quad_channel_hint.end())
                return it->second;
        }

        return graph::TextureChannelHint::RGBA;
    }

    void QuadResourcePrepareSystem::SetFixedSizeForWorld(const ECSContext* world, bool fixed)
    {
        if (!world)
            return;

        g_world_billboard_fixed_size[world] = fixed;
    }

    bool QuadResourcePrepareSystem::IsFixedSizeForWorld(const ECSContext* world)
    {
        if (world)
        {
            auto it = g_world_billboard_fixed_size.find(world);
            if (it != g_world_billboard_fixed_size.end())
                return it->second;
        }

        return true; // default: fixed size
    }

    graph::mtl::MaterialPreset QuadResourcePrepareSystem::GetBillboardPresetForWorld(const ECSContext* world)
    {
        return IsFixedSizeForWorld(world)
            ? graph::mtl::MaterialPreset::Billboard2DFixed
            : graph::mtl::MaterialPreset::Billboard2DDynamic;
    }

    void QuadResourcePrepareSystem::SetPreset(graph::GraphicsPipelinePreset preset)
    {
        g_default_quad_inline_pipeline = preset;
    }

    graph::GraphicsPipelinePreset QuadResourcePrepareSystem::GetPreset()
    {
        return g_default_quad_inline_pipeline;
    }

    void QuadResourcePrepareSystem::SetPipeline(graph::GraphicsPipelinePreset pipeline)
    {
        SetPreset(pipeline);
    }

    graph::GraphicsPipelinePreset QuadResourcePrepareSystem::GetPipeline()
    {
        return GetPreset();
    }

    graph::RenderAlphaMode QuadResourcePrepareSystem::GetBlendMode()
    {
        return GraphicsPipelinePresetToBlendMode(g_default_quad_inline_pipeline);
    }

    // ────────────────────────────────────────────────────────────────
    // Domain texture array management  (delegates to TextureDomainRegistry)
    // ────────────────────────────────────────────────────────────────

    int QuadResourcePrepareSystem::RegisterDomainTexture(const std::string& domain_tag,
                                                          const hgl::OSString& texture_path)
    {
        return graph::TextureDomainRegistry::RegisterTexture(domain_tag, texture_path);
    }

    QuadResourcePrepareSystem::DomainResources*
    QuadResourcePrepareSystem::GetDomainResources(const std::string& domain_tag)
    {
        return graph::TextureDomainRegistry::GetEntry(domain_tag);
    }

    // ────────────────────────────────────────────────────────────────

    QuadResourcePrepareSystem::QuadResourcePrepareSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderResourceSetup);
        SetRenderElementType("Billboard");
        AddDependency<RenderTargetSystem>();
    }

    void QuadResourcePrepareSystem::Update(float deltaTime)
    {
        if (!world)
            return;

        // Ensure shared resources are created and up-to-date
        EnsureSharedResources();

        // Build/rebuild domain texture arrays as needed
        EnsureDomainResources();
    }

    void QuadResourcePrepareSystem::Shutdown()
    {
        if (world)
        {
            g_world_quad_inline_pipeline.erase(world);
            g_world_quad_channel_hint.erase(world);
        }

        ReleaseDomainResources();
        ReleaseSharedResources();
        System::Shutdown();
    }

    bool QuadResourcePrepareSystem::EnsureSharedResources()
    {
        if (!world)
            return false;

        auto* render_context = world->GetRenderContext();
        auto* graphics_context = world->GetGraphicsContext();
        if (!render_context || !graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* recipe_registry = graphics_context->GetMaterialAssetRegistry();
        auto* device = graphics_context->GetDevice();
        if (!material_manager || !primitive_manager || !recipe_registry || !device)
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderFormat() : nullptr;
        if (!render_pass)
            return false;

        // If resources already exist and render pass hasn't changed, we're done
        if (shared_primitive && shared_render_pass == render_pass)
            return true;

        // Create shared material instance for billboard rendering
        const bool fixed = IsFixedSizeForWorld(world);
        const auto billboard_preset = GetBillboardPresetForWorld(world);

        graph::mtl::MaterialRecipe rec;
        rec.id = "quad_shared_billboard";
        rec.preset = billboard_preset;
        rec.dim = graph::mtl::MaterialRecipe::Dim::D3;
        rec.prim = graph::PrimitiveType::Billboard;
        rec.pipeline = GetPresetForWorld(world);
        rec.billboard.fixed_size = fixed;
        rec.billboard.blend_mode = GetBlendModeForWorld(world);
        rec.billboard.base_color_channel = GetChannelHintForWorld(world);
        rec.billboard.texture_id = "quad_shared";
        rec.textures = {
            { graph::mtl::SamplerSlot::BaseColor, graph::mtl::TextureSourceMode::Simple, "" },
        };

        graph::MaterialDomainHandle handle;
        shared_material_instance = recipe_registry->ResolveOrCreateBindingInstance(rec, nullptr, 0, &handle);
        if (!shared_material_instance || !handle.material)
            return false;

        auto* shared_material = handle.material;

        shared_material_instance->SetRenderPreset(GetPresetForWorld(world));

        // Create shared quad geometry (explicit quad for VS/FS-only billboard path)
        graph::GeometryVertexFormat quad_gvf;
        quad_gvf.Set(graph::VAN::Position, VF_V3F);
        auto pc = std::make_unique<graph::GeometryCreater>(device, quad_gvf);
        pc->Init("Quad", 4, 6, graph::IndexType::U16);

        static const float position_data[12] =
        {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };
        static const uint16_t index_data[6] = { 0, 1, 2, 0, 2, 3 };

        if (!pc->WriteVAB(graph::VAN::Position, VF_V3F, position_data))
            return false;

        if (!pc->WriteIBO(index_data))
            return false;

        shared_primitive = primitive_manager->CreatePrimitive(pc.get(), shared_material_instance);
        if (!shared_primitive)
            return false;

        // Create shared sampler
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!sampler_manager)
            return false;

        if (!shared_sampler)
            shared_sampler = sampler_manager->CreateSampler();

        if (!shared_sampler)
            return false;

        shared_render_pass = render_pass;
        return true;
    }

    void QuadResourcePrepareSystem::ReleaseSharedResources()
    {
        if (!world)
            return;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* material_manager = graphics_context->GetMaterialManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();

        graph::Geometry* geometry = nullptr;
        if (shared_primitive)
            geometry = shared_primitive->GetGeometry();

        if (shared_primitive && primitive_manager)
            primitive_manager->Release(shared_primitive);

        if (geometry)
            delete geometry;

        if (shared_material_instance && material_manager)
            material_manager->Destroy(shared_material_instance);

        if (shared_sampler && sampler_manager)
            sampler_manager->Release(shared_sampler);

        shared_primitive = nullptr;
        shared_material_instance = nullptr;
        shared_render_pass = nullptr;
        shared_sampler = nullptr;
    }

    bool QuadResourcePrepareSystem::EnsureDomainResources()
    {
        if (!world)
            return false;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device            = graphics_context->GetDevice();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* material_manager  = graphics_context->GetMaterialManager();
        if (!device || !primitive_manager || !material_manager)
            return false;

        // Build material / DMB for each dirty domain, then let the registry
        // handle Texture2DArray creation and descriptor binding.
        auto build_material_cb = [this, graphics_context, device, primitive_manager, material_manager]
            (const std::string& domain_tag,
             graph::TextureDomainRegistry::DomainEntry& entry,
             graph::GraphicsContext*) -> bool
        {
            const bool domain_fixed   = IsFixedSizeForWorld(world);
            const auto domain_preset  = GetBillboardPresetForWorld(world);
            const auto blend_mode     = GetBlendModeForWorld(world);
            const auto channel_hint   = GetChannelHintForWorld(world);

            std::fprintf(stderr, "[QuadResPrepare] EnsureDomainResources domain='%s'  blend=%d  fixed=%d  preset=%d\n",
                domain_tag.c_str(), (int)blend_mode, (int)domain_fixed, (int)domain_preset);

            auto* recipe_registry = graphics_context->GetMaterialAssetRegistry();
            if (!recipe_registry)
                return false;

            graph::mtl::MaterialRecipe rec;
            rec.id        = "billboard_domain_" + domain_tag;
            rec.domain_id = domain_tag;
            rec.preset    = domain_preset;
            rec.billboard.texture_id         = domain_tag;
            rec.billboard.blend_mode         = blend_mode;
            rec.billboard.base_color_channel = channel_hint;
            rec.billboard.fixed_size         = domain_fixed;
            rec.dim       = graph::mtl::MaterialRecipe::Dim::D3;
            rec.prim      = graph::PrimitiveType::Billboard;
            rec.pipeline  = GetPresetForWorld(world);
            rec.textures  = {
                { graph::mtl::SamplerSlot::BaseColor, graph::mtl::TextureSourceMode::Array, "" },
            };

            graph::MaterialDomainHandle handle = recipe_registry->Acquire(rec);
            if (!handle.IsValid())
            {
                std::fprintf(stderr, "[QuadResPrepare] registry.Acquire FAILED for domain '%s'\n",
                    domain_tag.c_str());
                return false;
            }

            entry.dmb      = handle.binding;
            entry.material = handle.material;
            entry.material->SetTextureArraySlotFlags(
                uint8_t(1u << uint8_t(graph::mtl::SamplerSlot::BaseColor)));
            std::fprintf(stderr, "[QuadResPrepare] registry.Acquire OK for domain '%s'\n",
                domain_tag.c_str());
            return true;
        };

        const bool all_ok = graph::TextureDomainRegistry::EnsureResources(
            graphics_context, build_material_cb);

        // Post-pass: register descriptors and create shared primitives for newly built domains.
        auto desc_sys = world->GetSystem<RenderDescriptorBindingSystem>();

        graph::TextureDomainRegistry::ForEach([&](const std::string& tag,
                                                   graph::TextureDomainRegistry::DomainEntry& dr)
        {
            if (!dr.material || !dr.dmb || !dr.texture_array)
                return;

            if (desc_sys)
            {
                desc_sys->RegisterDomainBinding(dr.dmb);
                desc_sys->RegisterDomainTextureSampler(dr.dmb,
                                                       graph::mtl::SamplerSlot::BaseColor,
                                                       dr.texture_array,
                                                       dr.sampler);
                desc_sys->RegisterMaterialTextureSampler(dr.material,
                                                         graph::mtl::SamplerSlot::BaseColor,
                                                         dr.texture_array,
                                                         dr.sampler);
            }

            if (!dr.primitive)
            {
                graph::MaterialInstanceSpec mi_spec;
                mi_spec.material = dr.material;
                mi_spec.domain   = dr.dmb ? dr.dmb->GetDomain() : nullptr;
                mi_spec.preset   = GetPresetForWorld(world);
                auto* temp_mi = material_manager->AcquireMaterialInstance(mi_spec);
                if (temp_mi)
                {
                    graph::GeometryVertexFormat quad_gvf;
                    quad_gvf.Set(graph::VAN::Position, VF_V3F);
                    auto pc = std::make_unique<graph::GeometryCreater>(device, quad_gvf);
                    pc->Init(AnsiString(("DomainQuad_" + tag).c_str()), 4, 6, graph::IndexType::U16);

                    static const float position_data[12] =
                    {
                        -0.5f, -0.5f, 0.0f,
                         0.5f, -0.5f, 0.0f,
                         0.5f,  0.5f, 0.0f,
                        -0.5f,  0.5f, 0.0f
                    };
                    static const uint16_t index_data[6] = { 0, 1, 2, 0, 2, 3 };

                    pc->WriteVAB(graph::VAN::Position, VF_V3F, position_data);
                    pc->WriteIBO(index_data);

                    dr.primitive = primitive_manager->CreatePrimitive(pc.get(), temp_mi);
                }
            }
        });

        return all_ok;
    }

    void QuadResourcePrepareSystem::ReleaseDomainResources()
    {
        if (!world)
            return;

        auto* graphics_context = world->GetGraphicsContext();

        if (auto desc_sys = world->GetSystem<RenderDescriptorBindingSystem>())
        {
            graph::TextureDomainRegistry::ForEach([&](const std::string&,
                                                       graph::TextureDomainRegistry::DomainEntry& dr)
            {
                if (dr.dmb)
                {
                    desc_sys->ClearDomainBindings(dr.dmb);
                    desc_sys->UnregisterDomainBinding(dr.dmb);
                }
            });
        }

        graph::TextureDomainRegistry::ReleaseAll(graphics_context);
    }

}//namespace hgl::ecs
