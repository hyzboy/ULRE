#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialAssetRecord.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKFormat.h>
#include<cstdint>
#include<unordered_map>

namespace hgl::ecs
{
    // Static member initialization
    graph::Primitive* QuadResourcePrepareSystem::shared_primitive = nullptr;
    graph::MaterialInstance* QuadResourcePrepareSystem::shared_material_instance = nullptr;
    graph::RenderTargetFormat* QuadResourcePrepareSystem::shared_render_pass = nullptr;
    graph::Sampler* QuadResourcePrepareSystem::shared_sampler = nullptr;

    std::unordered_map<std::string, QuadResourcePrepareSystem::DomainResources>
        QuadResourcePrepareSystem::s_domain_resources;

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
    // Domain texture array management
    // ────────────────────────────────────────────────────────────────

    constexpr uint32_t kDefaultDomainMaxLayers = 256;

    int QuadResourcePrepareSystem::RegisterDomainTexture(const std::string& domain_tag,
                                                          const hgl::OSString& texture_path)
    {
        auto& dr = s_domain_resources[domain_tag];
        if (dr.domain_tag.empty())
        {
            dr.domain_tag  = domain_tag;
            dr.max_layers  = kDefaultDomainMaxLayers;
            dr.used_layers = 0;
            dr.dirty       = true;
        }

        auto it = dr.path_to_layer.find(texture_path);
        if (it != dr.path_to_layer.end())
            return static_cast<int>(it->second);

        if (dr.used_layers >= dr.max_layers)
            return -1; // capacity full

        uint32_t layer = dr.used_layers++;
        dr.path_to_layer[texture_path] = layer;
        dr.dirty = true;
        return static_cast<int>(layer);
    }

    QuadResourcePrepareSystem::DomainResources*
    QuadResourcePrepareSystem::GetDomainResources(const std::string& domain_tag)
    {
        auto it = s_domain_resources.find(domain_tag);
        return (it != s_domain_resources.end()) ? &it->second : nullptr;
    }

    // ────────────────────────────────────────────────────────────────

    QuadResourcePrepareSystem::QuadResourcePrepareSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::MaterialTemplate);
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
        auto* device = graphics_context->GetDevice();
        if (!material_manager || !primitive_manager || !device)
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

        graph::mtl::BillboardMaterialCreateConfig cfg(graph::PrimitiveType::Billboard);
        cfg.fixed_size  = fixed;
        cfg.blend_mode  = GetBlendModeForWorld(world);
        cfg.base_color_channel = GetChannelHintForWorld(world);

        auto* shared_material = material_manager->AcquireMaterial(billboard_preset, &cfg);
        if (!shared_material)
            return false;

        graph::MaterialInstanceSpec spec;
        spec.material = shared_material;
        spec.preset = GetPresetForWorld(world);
        shared_material_instance = material_manager->AcquireMaterialInstance(spec);
        if (!shared_material_instance)
            return false;

        // Phase B: use material's VIL directly instead of MI getter
        const graph::VIL *use_vil = shared_material->GetDefaultVIL();

        // Create shared quad geometry (explicit quad for VS/FS-only billboard path)
        auto pc = std::make_unique<graph::GeometryCreater>(device, use_vil);
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

        auto* render_context    = world->GetRenderContext();
        auto* graphics_context  = world->GetGraphicsContext();
        if (!render_context || !graphics_context)
            return false;

        auto* material_manager  = graphics_context->GetMaterialManager();
        auto* texture_manager   = graphics_context->GetTextureManager();
        auto* sampler_manager   = graphics_context->GetSamplerManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* device            = graphics_context->GetDevice();
        if (!material_manager || !texture_manager || !sampler_manager || !primitive_manager || !device)
            return false;

        bool all_ok = true;

        for (auto& [tag, dr] : s_domain_resources)
        {
            if (!dr.dirty && dr.texture_array)
                continue; // already built and up-to-date

            if (dr.path_to_layer.empty())
                continue; // no textures registered yet

            // ── Texture2DArray ────────────────────────────────────
            // Need to (re)create when dirty. For simplicity we always
            // recreate; a production path would grow/stream layers.
            if (dr.texture_array)
            {
                texture_manager->Destory(dr.texture_array);
                dr.texture_array = nullptr;
            }

            // Detect format / size from the first texture file
            // Load first texture as Texture2D to probe dimensions
            auto first_it = dr.path_to_layer.begin();
            auto* probe = texture_manager->LoadTexture2D(first_it->first, false);
            if (!probe)
            {
                all_ok = false;
                continue;
            }

            const uint32_t tex_w  = probe->GetWidth();
            const uint32_t tex_h  = probe->GetHeight();
            const VkFormat tex_fmt = probe->GetFormat();

            dr.texture_array = texture_manager->CreateTexture2DArray(
                tex_w, tex_h,
                dr.used_layers,
                tex_fmt,
                false);

            if (!dr.texture_array)
            {
                all_ok = false;
                continue;
            }

            // Load each layer
            for (auto& [path, layer] : dr.path_to_layer)
            {
                if (!texture_manager->LoadTexture2DArray(dr.texture_array, layer, path))
                {
                    all_ok = false;
                }
            }

            // ── Sampler ───────────────────────────────────────────
            if (!dr.sampler)
                dr.sampler = sampler_manager->CreateSampler();

            // ── MaterialTemplate + DMB via MaterialAssetRegistry ──────────
            if (!dr.material || !dr.dmb)
            {
                const bool domain_fixed = IsFixedSizeForWorld(world);
                const auto domain_preset = GetBillboardPresetForWorld(world);

                graph::mtl::BillboardMaterialCreateConfig cfg(graph::PrimitiveType::Billboard);
                cfg.fixed_size         = domain_fixed;
                cfg.blend_mode         = GetBlendModeForWorld(world);
                cfg.base_color_channel = GetChannelHintForWorld(world);
                cfg.texture_id         = dr.domain_tag;
                cfg.use_texture_array  = true;

                std::fprintf(stderr, "[QuadResPrepare] EnsureDomainResources domain='%s'  use_texture_array=%d  blend=%d  fixed=%d  preset=%d\n",
                    dr.domain_tag.c_str(), (int)cfg.use_texture_array, (int)cfg.blend_mode, (int)domain_fixed, (int)domain_preset);

                dr.material = material_manager->AcquireMaterial(domain_preset, &cfg);

                if (dr.material)
                {
                    // Mark material as using texture array on BaseColor slot
                    dr.material->SetTextureArraySlotFlags(
                        uint8_t(1u << uint8_t(graph::mtl::SamplerSlot::BaseColor)));

                    // Create DMB via registry
                    graph::MaterialAssetRegistry registry(material_manager, texture_manager, sampler_manager);

                    graph::mtl::MaterialAssetRecord rec;
                    rec.id        = "billboard_domain_" + dr.domain_tag;
                    rec.domain_id = dr.domain_tag;
                    rec.preset    = domain_preset;
                    rec.billboard.texture_id       = dr.domain_tag;
                    rec.billboard.blend_mode        = cfg.blend_mode;
                    rec.billboard.base_color_channel = cfg.base_color_channel;
                    rec.billboard.fixed_size        = domain_fixed;
                    rec.dim       = graph::mtl::MaterialAssetRecord::Dim::D3;
                    rec.prim      = graph::PrimitiveType::Billboard;
                    rec.pipeline  = GetPresetForWorld(world);
                    rec.textures  = {
                        { graph::mtl::SamplerSlot::BaseColor, graph::mtl::TextureSourceMode::Array, "" },
                    };

                    auto handle = registry.Acquire(rec);
                    if (handle.IsValid())
                    {
                        dr.dmb      = handle.binding;
                        dr.material = handle.material;
                        dr.material->SetTextureArraySlotFlags(
                            uint8_t(1u << uint8_t(graph::mtl::SamplerSlot::BaseColor)));
                        std::fprintf(stderr, "[QuadResPrepare] registry.Acquire OK, material replaced by handle.material\n");
                    }
                    else
                    {
                        std::fprintf(stderr, "[QuadResPrepare] registry.Acquire FAILED for domain '%s'\n",
                            dr.domain_tag.c_str());
                    }
                }
            }

            if (!dr.material || !dr.dmb)
            {
                all_ok = false;
                continue;
            }

            // Bind texture array to the domain's PerMaterial descriptor set
            dr.dmb->BindTextureSampler(graph::mtl::SamplerSlot::BaseColor,
                                       dr.texture_array,
                                       dr.sampler);
            dr.dmb->Update();

            // Also bind to the MaterialTemplate's own descriptor set — this is what
            // the command buffer actually binds at draw time.
            dr.material->BindTextureSampler(graph::mtl::SamplerSlot::BaseColor,
                                            dr.texture_array,
                                            dr.sampler);
            dr.material->Update();

            // Register with descriptor binding system for per-frame sync
            if (auto desc_sys = world->GetSystem<RenderDescriptorBindingSystem>())
            {
                desc_sys->RegisterDomainBinding(dr.dmb);
                desc_sys->RegisterDomainTextureSampler(dr.dmb,
                                                       graph::mtl::SamplerSlot::BaseColor,
                                                       dr.texture_array,
                                                       dr.sampler);
                // MaterialTemplate-level binding so per-frame sync picks it up
                desc_sys->RegisterMaterialTextureSampler(dr.material,
                                                         graph::mtl::SamplerSlot::BaseColor,
                                                         dr.texture_array,
                                                         dr.sampler);
            }

            // ── Shared primitive for this domain ─────────────────────
            if (!dr.primitive)
            {
                // Phase B: use material's VIL directly instead of creating temp MI
                const graph::VIL *use_vil = dr.material->GetDefaultVIL();
                
                graph::MaterialInstanceSpec mi_spec;
                mi_spec.material = dr.material;
                mi_spec.preset   = GetPresetForWorld(world);
                auto* temp_mi = material_manager->AcquireMaterialInstance(mi_spec);

                if (temp_mi)
                {
                    auto pc = std::make_unique<graph::GeometryCreater>(device, use_vil);
                    pc->Init(AnsiString(("DomainQuad_" + dr.domain_tag).c_str()), 4, 6, graph::IndexType::U16);

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

            dr.dirty = false;
        }

        return all_ok;
    }

    void QuadResourcePrepareSystem::ReleaseDomainResources()
    {
        if (!world)
            return;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return;

        auto* texture_manager   = graphics_context->GetTextureManager();
        auto* sampler_manager   = graphics_context->GetSamplerManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();

        if (auto desc_sys = world->GetSystem<RenderDescriptorBindingSystem>())
        {
            for (auto& [tag, dr] : s_domain_resources)
            {
                if (dr.dmb)
                {
                    desc_sys->ClearDomainBindings(dr.dmb);
                    desc_sys->UnregisterDomainBinding(dr.dmb);
                }
            }
        }

        for (auto& [tag, dr] : s_domain_resources)
        {
            if (dr.primitive && primitive_manager)
            {
                auto* geometry = dr.primitive->GetGeometry();
                primitive_manager->Release(dr.primitive);
                if (geometry) delete geometry;
            }

            if (dr.texture_array && texture_manager)
                texture_manager->Destory(dr.texture_array);

            if (dr.sampler && sampler_manager)
                sampler_manager->Release(dr.sampler);
        }

        s_domain_resources.clear();
    }
}//namespace hgl::ecs
