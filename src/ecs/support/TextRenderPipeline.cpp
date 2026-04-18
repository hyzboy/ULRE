#include<hgl/ecs/support/TextRenderPipeline.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/font/TextGeometry.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/tile/TileData.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/common/RenderOptions.h>
#include<hgl/type/String.h>
#include<hgl/type/MemoryUtil.h>
#include<hgl/type/AlignUtil.h>
#include<cmath>

namespace hgl::ecs
{
    namespace
    {
        struct TextResolvedMaterialState
        {
            graph::MaterialBindingInstance *binding_instance = nullptr;
            graph::ShaderMaterialProgram *material = nullptr;
            const graph::VIL *vil = nullptr;
            graph::GraphicsPipelinePreset preset = graph::GraphicsPipelinePreset::Solid2D;
        };

        TextResolvedMaterialState ResolveTextMaterialState(graph::ShaderMaterialProgram *fallback_material,
                                                           graph::MaterialBindingInstance *mi)
        {
            TextResolvedMaterialState state{};
            state.binding_instance = mi;
            state.material = fallback_material;

            if (!mi)
                return state;

            state.vil = mi->GetVIL();
            state.preset = mi->GetRenderPreset();

            if (auto *mi_material = mi->GetShaderMaterialProgram())
            {
#ifdef _DEBUG
                if (state.material && state.material != mi_material)
                {
                    GLogWarning("[TextRenderPipeline] Unified-state mismatch: fallback material=%p mi.material=%p",
                                static_cast<void *>(state.material),
                                static_cast<void *>(mi_material));
                }
#endif
                state.material = mi_material;
            }

            return state;
        }

        graph::TileFont* CreateTileFont(graph::RenderContext* rc,
                                        graph::FontSource* fs,
                                        int limit_count,
                                        const VkExtent2D* extent)
        {
            if (!rc || !fs)
                return nullptr;

            auto* gc = rc->GetGraphicsContext();
            if (!gc)
                return nullptr;

            const uint32_t height = hgl_align_pow2(fs->GetCharHeight() + 2, 4);

            if (limit_count <= 0)
            {
                VkExtent2D ext{1024, 1024};
                if (extent)
                    ext = *extent;

                limit_count = (ext.width / height) * (ext.height / height);
                if (limit_count <= 0)
                    limit_count = 1024;
            }

            auto tm = gc->GetTextureManager();
            if (!tm)
                return nullptr;

            auto* td = tm->CreateTileData(UPF_R8, height, height, limit_count);
            if (!td)
                return nullptr;

            return new graph::TileFont(td, fs);
        }

        void BuildDrawStyle(graph::layout::TextDrawStyle& out_style,
                            const graph::layout::ParagraphStyle& para_style,
                            const graph::layout::TEXT_COORD_VEC& start_pos,
                            const int char_height)
        {
            out_style.para_style = para_style;
            out_style.start_position = start_pos;

            const float origin_char_height = static_cast<float>(char_height);

            out_style.char_height = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height));
            out_style.space_size = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.space_size));
            out_style.full_space_size = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.full_space_size));
            out_style.tab_size = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.tab_size));
            out_style.char_gap = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.char_gap));
            out_style.line_gap = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.line_gap));
            out_style.line_height = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height + out_style.line_gap));
        }

        graph::ResourceDomain *ResolveDomainForMaterial(graph::ShaderMaterialProgramManager *material_manager,
                                                        graph::ShaderMaterialProgram *material,
                                                        uint32_t domain_id)
        {
            if (!material)
                return nullptr;

            auto *gc = material_manager ? material_manager->GetGraphicsContext() : nullptr;
            auto *rdm = gc ? gc->GetResourceDomainManager() : nullptr;
            if (!rdm)
                return nullptr;

            const auto schema = material->GetShaderDataSchema();
            if (auto *domain = rdm->Get(schema, domain_id))
                return domain;

            graph::ResourceDomainCreateInfo ci;
            ci.schema = schema;
            ci.domain_id = domain_id;
            ci.initial_capacity = 1024;
            return rdm->Create(ci);
        }
    }

    TextRenderPipeline::~TextRenderPipeline()
    {
        if (!render_context && world)
            render_context = world->GetRenderContext();

        auto* graphics_context = render_context ? render_context->GetGraphicsContext() : nullptr;
        auto* primitive_manager = graphics_context ? graphics_context->GetPrimitiveManager() : nullptr;
        auto* material_manager = graphics_context ? graphics_context->GetMaterialManager() : nullptr;
        auto* sampler_manager = graphics_context ? graphics_context->GetSamplerManager() : nullptr;
        auto* buffer_manager = graphics_context ? graphics_context->GetBufferManager() : nullptr;
        auto descriptor_binding_system = world ? world->GetSystem<RenderDescriptorBindingSystem>() : nullptr;

        for (auto& pair : resources_by_font)
        {
            auto& res = pair.second;

            if (res.geometry)
            {
                delete res.geometry;
                res.geometry = nullptr;
            }

            if (res.primitive && primitive_manager)
            {
                primitive_manager->Release(res.primitive);
                res.primitive = nullptr;
            }

            if (res.material_instance && material_manager)
            {
                material_manager->Release(res.material_instance);
                res.material_instance = nullptr;
            }

            if (res.material && material_manager)
            {
                if (descriptor_binding_system)
                {
                    descriptor_binding_system->UnregisterPipelineMaterial(res.material);
                    descriptor_binding_system->ClearMaterialBindings(res.material);
                }

                material_manager->Release(res.material);
                res.material = nullptr;
            }

            if (res.sampler && sampler_manager)
            {
                sampler_manager->Release(res.sampler);
                res.sampler = nullptr;
            }

            if (res.material_instance_buffer && buffer_manager)
            {
                buffer_manager->Release(res.material_instance_buffer);
                res.material_instance_buffer = nullptr;
            }

            if (res.tile_font)
            {
                delete res.tile_font;
                res.tile_font = nullptr;
            }
        }
        resources_by_font.Clear();
    }

    bool TextRenderPipeline::PrepareFrame()
    {
        if (!world)
            return false;

        const uint32_t frame_index = world->GetFrameIndex();
        if (prepared_frame_index == frame_index)
            return true;

        frame_texts.clear();
        frame_inputs.clear();

        world->GetComponents<TextComponent>(frame_texts);

        if (!PrepareFrameResources(frame_graphics_context,
                                   frame_material_manager,
                                   frame_primitive_manager,
                                   frame_device,
                                   frame_render_target))
            return false;

        prepared_frame_index = frame_index;
        return true;
    }

    void TextRenderPipeline::RunCollect()
    {
        frame_inputs.clear();
        BuildInputs(frame_texts, frame_inputs);
    }

    void TextRenderPipeline::RunBuild()
    {
        ProcessInputs(frame_inputs,
                      frame_material_manager,
                      frame_primitive_manager,
                      frame_device);
    }

    void TextRenderPipeline::RunSync()
    {
        ClearChanges(frame_texts);
    }

    void TextRenderPipeline::GetRenderPrimitives(std::vector<graph::Primitive*>& out_primitives) const
    {
        for (const auto& pair : resources_by_font)
        {
            if (pair.second.primitive)
                out_primitives.push_back(pair.second.primitive);
        }
    }

    void TextRenderPipeline::GetRenderEntries(std::vector<RenderEntry>& out_entries) const
    {
        for (const auto& pair : resources_by_font)
        {
            const auto& res = pair.second;
            if (res.primitive && res.pipeline)
                out_entries.emplace_back(res.primitive, res.pipeline);
        }
    }

    TextRenderPipeline::RenderResources* TextRenderPipeline::GetOrCreateResources(graph::FontSource* font_source,
                                                                                   uint32_t estimate_chars)
    {
        if (!font_source)
            return nullptr;

        if (!render_context && world)
            render_context = world->GetRenderContext();

        auto* graphics_context = render_context ? render_context->GetGraphicsContext() : nullptr;
        if (!graphics_context && world)
            graphics_context = world->GetGraphicsContext();

        if (!render_context || !graphics_context)
            return nullptr;

        if (auto* entry = resources_by_font.GetValuePointer(font_source))
            return entry;

        RenderResources resources;
        graph::ShaderMaterialProgramManager* material_manager = nullptr;
        graph::SamplerManager* sampler_manager = nullptr;
        graph::BufferManager* buffer_manager = nullptr;

        struct BuildGuard
        {
            graph::ShaderMaterialProgramManager* material_manager = nullptr;
            graph::SamplerManager* sampler_manager = nullptr;
            graph::ShaderMaterialProgram* material = nullptr;
            graph::Sampler* sampler = nullptr;
            graph::BufferManager* buffer_manager = nullptr;
            graph::DeviceBuffer* material_instance_buffer = nullptr;
            std::unique_ptr<graph::TileFont> tile_font;
            bool committed = false;

            ~BuildGuard()
            {
                if (committed)
                    return;

                if (sampler && sampler_manager)
                    sampler_manager->Release(sampler);

                if (material_instance_buffer && buffer_manager)
                    buffer_manager->Release(material_instance_buffer);

                if (material && material_manager)
                    material_manager->Release(material);
            }
        } guard;

        const int limit_count = static_cast<int>((estimate_chars > 0) ? estimate_chars : 256);
        VkExtent2D extent{1024, 1024};
        if (world)
        {
            auto* target = world->GetRenderTarget();
            if (target)
                extent = target->GetExtent();
        }

        guard.tile_font.reset(CreateTileFont(render_context, font_source, limit_count, &extent));
        if (!guard.tile_font)
            return nullptr;

        graph::mtl::Text2DMaterialCreateConfig mtl_cfg;

        material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return nullptr;

        guard.material_manager = material_manager;

        guard.material = material_manager->ResolveOrCreateProgram(graph::mtl::MaterialPreset::Text2D, &mtl_cfg);
        if (!guard.material)
            return nullptr;

        sampler_manager = graphics_context->GetSamplerManager();
        if (!sampler_manager)
            return nullptr;

        guard.sampler_manager = sampler_manager;

        guard.sampler = sampler_manager->CreateSampler();
        if (!guard.sampler)
            return nullptr;

        buffer_manager = graphics_context->GetBufferManager();
        if (!buffer_manager)
            return nullptr;

        guard.buffer_manager = buffer_manager;

        const uint32_t mi_bytes = graph::mtl::GetShaderDataSchemaInfo(guard.material->GetShaderDataSchema()).byte_size;
        if (mi_bytes > 0)
        {
            guard.material_instance_buffer = buffer_manager->CreateSSBO("Text2D_MI", mi_bytes, graph::SharingMode::Exclusive);
            if (!guard.material_instance_buffer)
                return nullptr;

            if (!guard.material->BindSSBO(graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceData,
                                          guard.material_instance_buffer->GetGPUBuffer()))
                return nullptr;

            resources.material_instance_buffer = guard.material_instance_buffer;
            guard.material_instance_buffer = nullptr;
        }

        if (!guard.material->BindResourceSampler(graph::mtl::SamplerSlot::Text,
                            guard.tile_font->GetTexture(),
                            guard.sampler))
            return nullptr;

        if (world)
        {
            if (auto descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>())
            {
                descriptor_binding_system->RegisterPipelineMaterial(guard.material);
                descriptor_binding_system->RegisterMaterialTextureSampler(guard.material,
                                                                          graph::mtl::SamplerSlot::Text,
                                                                          guard.tile_font->GetTexture(),
                                                                          guard.sampler);
            }
        }

        resources.tile_font = guard.tile_font.release();
        resources.material = guard.material;
        resources.sampler = guard.sampler;
        guard.committed = true;

        resources_by_font.Add(font_source, resources);
        return resources_by_font.GetValuePointer(font_source);
    }

    bool TextRenderPipeline::ResolvePipelineForCurrentRenderTarget(RenderResources& resources,
                                                                   graph::VulkanDevice* device,
                                                                   graph::IRenderTarget* render_target)
    {
        if (!device || !render_target || !resources.material || !resources.material_instance)
            return false;

        const auto state = ResolveTextMaterialState(resources.material, resources.material_instance);
        if (!state.material || !state.vil)
            return false;

        auto* render_format = render_target->GetRenderFormat();
        if (!render_format)
            return false;

        if (resources.pipeline && resources.render_format == render_format)
            return true;

        const graph::GraphicsPipelinePreset preset = state.preset;
        const graph::GraphicsPipelineData* pipeline_data = graph::GetGraphicsPipelineData(preset);
        if (!pipeline_data)
            return false;

        graph::GraphicsPipelineBuildRequest req;
        req.material = state.material;
        req.vil = state.vil;
        req.render_format = render_format;
        req.pipeline_data = pipeline_data;
        req.primitive = state.material->GetPrimitiveType();
        req.primitive_restart = (pipeline_data->input_assembly.primitiveRestartEnable == VK_TRUE);

        auto* resolved = device->AcquireGraphicsPipeline(req);
        if (!resolved)
            return false;

        resources.pipeline = resolved;
        resources.render_format = render_format;

        return true;
    }

    bool TextRenderPipeline::PrepareFrameResources(graph::GraphicsContext*& graphics_context,
                                                   graph::ShaderMaterialProgramManager*& material_manager,
                                                   graph::PrimitiveManager*& primitive_manager,
                                                   graph::VulkanDevice*& device,
                                                   graph::IRenderTarget*& render_target)
    {
        if (!world)
            return false;

        if (!render_context)
            render_context = world->GetRenderContext();

        if (!render_context)
            return false;

        graphics_context = render_context ? render_context->GetGraphicsContext() : nullptr;
        if (!graphics_context && world)
            graphics_context = world->GetGraphicsContext();

        device = graphics_context ? graphics_context->GetDevice() : nullptr;
        if (!device)
            return false;

        material_manager = graphics_context ? graphics_context->GetMaterialManager() : nullptr;
        primitive_manager = graphics_context ? graphics_context->GetPrimitiveManager() : nullptr;

        render_target = render_context->GetCurrentRenderTarget();
        if (!render_target && world)
            render_target = world->GetRenderTarget();

        if (!material_manager || !primitive_manager || !render_target)
            return false;

        return true;
    }

    void TextRenderPipeline::BuildInputs(std::vector<std::shared_ptr<TextComponent>>& texts,
                                         std::unordered_map<graph::FontSource*, BatchInput>& inputs)
    {
        for (const auto& text_comp : texts)
        {
            if (!text_comp || text_comp->GetText().IsEmpty())
                continue;

            Entity* owner = text_comp->GetOwner();
            if (owner && world && !world->IsEntityRenderEnabled(owner))
                continue;

            auto* font_source = text_comp->GetFontSource();
            if (!font_source)
                continue;

            if (inputs.find(font_source) == inputs.end())
            {
                inputs[font_source] = BatchInput{font_source};
            }

            auto& input = inputs[font_source];
            input.texts.push_back(text_comp.get());
            input.total_chars += text_comp->GetText().Length();
            input.batch_style = text_comp->GetCharStyle();

            if (text_comp->GetChangeMask() != 0)
                input.dirty = true;
        }
    }

    void TextRenderPipeline::ProcessInputs(std::unordered_map<graph::FontSource*, BatchInput>& inputs,
                                           graph::ShaderMaterialProgramManager* material_manager,
                                           graph::PrimitiveManager* primitive_manager,
                                           graph::VulkanDevice* device)
    {
        for (auto& pair : inputs)
        {
            auto& input = pair.second;

            if (!input.font_source || input.texts.empty())
                continue;

            auto* resources = GetOrCreateResources(input.font_source, input.total_chars);
            if (!resources)
                continue;

            const bool font_changed = !resources->tile_font;
            const bool style_changed = font_changed || mem_compare(resources->char_style, input.batch_style) != 0;

            if (style_changed)
            {
                resources->char_style = input.batch_style;
                input.dirty = true;
            }

            graph::MaterialBindingInstance* mi = resources->material_instance;
            if (!mi)
            {
                graph::VILConfig vil_config;
                vil_config.Add(graph::VAN::Position, VF_V2I16);

                graph::MaterialInstanceSpec mi_spec;
                mi_spec.material = resources->material;
                mi_spec.vil_cfg = &vil_config;
                mi_spec.preset = graph::GraphicsPipelinePreset::Solid2D;
                mi_spec.domain = ResolveDomainForMaterial(material_manager, resources->material, 2001u);

                mi = material_manager->AcquireMaterialInstance(mi_spec);
                if (!mi)
                    continue;

                resources->material_instance = mi;
                input.dirty = true;
            }

            if (!ResolvePipelineForCurrentRenderTarget(*resources, device, frame_render_target))
                continue;

            if (input.dirty)
            {
                if (resources->material_instance_buffer)
                {
                    const uint32_t upload_bytes = hgl_min<uint32_t>(graph::mtl::GetShaderDataSchemaInfo(resources->material->GetShaderDataSchema()).byte_size,
                                                                    sizeof(graph::layout::CharStyle));
                    if(auto *mgpu = resources->material_instance_buffer->GetGPUBuffer())
                        mgpu->Write(&resources->char_style, 0, upload_bytes);
                }
            }

            graph::TextGeometry* geometry = resources->geometry;
            if (!geometry)
            {
                const uint32_t estimate = input.total_chars;
                const graph::GeometryVertexFormat gvf = graph::BuildTextGeometryVertexFormat();
                geometry = new graph::TextGeometry(device, gvf, estimate);
                resources->geometry = geometry;
            }

            const bool should_layout = input.dirty || geometry == nullptr ||
                                       resources->last_draw_char_count == 0 ||
                                       resources->last_string_count != static_cast<uint32_t>(input.texts.size());

            if (should_layout)
            {
                graph::layout::TextLayout layout_engine(resources->tile_font);
                if (layout_engine.Begin(geometry, input.total_chars))
                {
                    for (const auto* text_comp : input.texts)
                    {
                        graph::layout::TextDrawStyle draw_style;
                        BuildDrawStyle(draw_style,
                                       text_comp->GetParagraphStyle(),
                                       text_comp->GetStartPosition(),
                                       resources->tile_font->GetFontSource()->GetCharHeight());

                        layout_engine.AddString(text_comp->GetText(), draw_style);
                    }

                    const int draw_count = layout_engine.End();
                    if (draw_count > 0)
                    {
                        resources->last_draw_char_count = static_cast<uint32_t>(draw_count);

                        auto* prim = resources->primitive;
                        if (prim)
                            prim->UpdateGeometry();
                    }
                }
            }

            graph::Primitive* primitive = resources->primitive;
            if (!primitive)
            {
                primitive = primitive_manager->CreatePrimitive(geometry, mi, resources->pipeline);
                if (!primitive)
                    continue;

                resources->primitive = primitive;
            }

            resources->last_string_count = static_cast<uint32_t>(input.texts.size());
        }
    }

    void TextRenderPipeline::ClearChanges(const std::vector<std::shared_ptr<TextComponent>>& texts)
    {
        for (const auto& text_comp : texts)
        {
            if (text_comp)
                text_comp->ClearAllChanges();
        }
    }
}
