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
#include<hgl/vk/VKDevice.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>
#include<hgl/graph/tile/TileData.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKCommandBuffer.h>

#include<hgl/mtl/SamplerName.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/common/RenderOptions.h>
#include<hgl/type/String.h>
#include<hgl/type/MemoryUtil.h>
#include<hgl/type/AlignUtil.h>
#include<cmath>

namespace hgl::ecs
{
    namespace
    {
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

        uint32_t ResolveMaterialSSBOStride(const graph::ShaderProgram *material)
        {
            if (!material)
                return 0;

            for (const auto &req : material->GetShaderResourceSchema().resources)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialDataSlotData)
                    continue;

                return graph::mtl::GetSSBOTypeStructStride(req.ssbo_type);
            }

            return 0;
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
    }

    TextRenderPipeline::~TextRenderPipeline()
    {
        if (!render_context && world)
            render_context = world->GetRenderContext();

        auto* graphics_context = render_context ? render_context->GetGraphicsContext() : nullptr;
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

            SAFE_CLEAR(res.data_buffer);
            SAFE_CLEAR(res.draw_range);

            if (res.material && material_manager)
            {
                if (res.binding_vil)
                {
                    res.material->Release(res.binding_vil);
                    res.binding_vil = nullptr;
                }

                if (descriptor_binding_system)
                {
                    descriptor_binding_system->UnregisterPipelineMaterial(res.material);
                    descriptor_binding_system->ClearMaterialBindings(res.material);
                }

                material_manager->Release(res.material);
                res.material = nullptr;
            }

            if (res.descriptor_binding_set)
            {
                delete res.descriptor_binding_set;
                res.descriptor_binding_set = nullptr;
            }

            if (res.sampler && sampler_manager)
            {
                sampler_manager->Release(res.sampler);
                res.sampler = nullptr;
            }

            if (res.material_data_buffer && buffer_manager)
            {
                buffer_manager->Release(res.material_data_buffer);
                res.material_data_buffer = nullptr;
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
                                   frame_render_pass,
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
                      frame_render_pass,
                      frame_device);
    }

    void TextRenderPipeline::RunSync()
    {
        ClearChanges(frame_texts);
    }

    void TextRenderPipeline::Render(graph::RenderCmdBuffer* cmd)
    {
        if (!cmd)
            return;

        for (auto &pair : resources_by_font)
        {
            auto &res = pair.second;

            if (!res.pipeline || !res.material || !res.data_buffer || !res.draw_range || res.last_draw_char_count == 0)
                continue;

            cmd->BindPipeline(res.pipeline);
            cmd->BindDescriptorSets(res.material);
            cmd->BindDataBuffer(res.data_buffer);
            cmd->Draw(res.data_buffer, res.draw_range);
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
        graph::ShaderProgramManager* material_manager = nullptr;
        graph::SamplerManager* sampler_manager = nullptr;
        graph::BufferManager* buffer_manager = nullptr;

        struct BuildGuard
        {
            graph::ShaderProgramManager* material_manager = nullptr;
            graph::SamplerManager* sampler_manager = nullptr;
            graph::ShaderProgram* material = nullptr;
            graph::VIL* binding_vil = nullptr;
            graph::DescriptorBindingSet* descriptor_binding_set = nullptr;
            graph::Sampler* sampler = nullptr;
            graph::BufferManager* buffer_manager = nullptr;
            graph::DeviceBuffer* material_data_buffer = nullptr;
            std::unique_ptr<graph::TileFont> tile_font;
            bool committed = false;

            ~BuildGuard()
            {
                if (committed)
                    return;

                if (sampler && sampler_manager)
                    sampler_manager->Release(sampler);

                if (material_data_buffer && buffer_manager)
                    buffer_manager->Release(material_data_buffer);

                if (descriptor_binding_set)
                    delete descriptor_binding_set;

                if (material && binding_vil)
                    material->Release(binding_vil);

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

        graph::mtl::MaterialRecipe recipe{};
        recipe.mtl_def_id = "Text2D";
        recipe.render_state_overrides.pipeline_config = graph::mtl::MakeSolid2DConfig();
        const graph::GeometryVertexFormat text_gvf = graph::CreateTextGeometryVertexFormat();

        material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return nullptr;

        guard.material_manager = material_manager;

        {
            graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
            mtl_request.recipe = recipe;
            mtl_request.primitive_type = graph::PrimitiveType::Triangles;
            mtl_request.geometry_vertex_format = &text_gvf;
            guard.material = material_manager->AcquireShaderProgram(mtl_request);
        }
        if (!guard.material)
            return nullptr;

        {
            guard.binding_vil = guard.material->CreateVIL(text_gvf);
            if (!guard.binding_vil)
                return nullptr;
        }

        guard.descriptor_binding_set = new graph::DescriptorBindingSet(guard.material, guard.binding_vil);
        if (!guard.descriptor_binding_set)
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

        const uint32_t mi_bytes = ResolveMaterialSSBOStride(guard.material);
        if (mi_bytes > 0)
        {
            guard.material_data_buffer = buffer_manager->CreateSSBO("Text2D_MaterialData", mi_bytes, graph::SharingMode::Exclusive);
            if (!guard.material_data_buffer)
                return nullptr;

            if (!guard.material->BindSSBO(graph::DescriptorSetType::Material,
                                          graph::mtl::DefaultMaterialDataSlotName,
                                          guard.material_data_buffer->GetGPUBuffer()))
                return nullptr;

            resources.material_data_buffer = guard.material_data_buffer;

            auto *domain_manager = graphics_context->GetResourceDomainManager();
            if (!domain_manager)
                return nullptr;

            for (const auto &req : guard.material->GetShaderResourceSchema().resources)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialDataSlotData)
                    continue;

                const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                if (!domain_manager->RegisterBuffer(addr, guard.material_data_buffer, 1))
                    return nullptr;

                if (!guard.descriptor_binding_set->SetSSBOBinding(req.ssbo_type, req.ssbo_id, 0))
                    return nullptr;
            }

            guard.material_data_buffer = nullptr;
        }

        if (!guard.material->BindTextureSampler(graph::DescriptorSetType::Material,
                                                    graph::mtl::SamplerName::Text,
                                                    guard.tile_font->GetTexture(),
                                                    guard.sampler))
            return nullptr;

        if (world)
        {
            if (auto descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>())
            {
                descriptor_binding_system->RegisterPipelineMaterial(guard.material);
                descriptor_binding_system->RegisterMaterialTextureSampler(guard.material,
                                                                          graph::mtl::SamplerName::Text,
                                                                          guard.tile_font->GetTexture(),
                                                                          guard.sampler);
            }
        }

        resources.tile_font = guard.tile_font.release();
        resources.material = guard.material;
        resources.binding_vil = guard.binding_vil;
        resources.descriptor_binding_set = guard.descriptor_binding_set;
        resources.sampler = guard.sampler;
        guard.binding_vil = nullptr;
        guard.descriptor_binding_set = nullptr;
        guard.committed = true;

        resources_by_font.Add(font_source, resources);
        return resources_by_font.GetValuePointer(font_source);
    }

    bool TextRenderPipeline::PrepareFrameResources(graph::GraphicsContext*& graphics_context,
                                                   graph::ShaderProgramManager*& material_manager,
                                                   graph::RenderPass*& render_pass,
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

        render_target = render_context->GetCurrentRenderTarget();
        if (!render_target && world)
            render_target = world->GetRenderTarget();

        render_pass = render_target ? render_target->GetRenderPass() : nullptr;

        if (!material_manager || !render_pass)
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
                                           graph::RenderPass* render_pass,
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

            auto *binding_set = resources->descriptor_binding_set;
            if (!binding_set || !resources->binding_vil)
                continue;

            if (input.dirty)
            {
                if (resources->material_data_buffer)
                {
                    const uint32_t upload_bytes = hgl_min<uint32_t>(ResolveMaterialSSBOStride(resources->material),
                                                                    sizeof(graph::layout::CharStyle));
                    if(auto *mgpu = resources->material_data_buffer->GetGPUBuffer())
                        mgpu->Write(&resources->char_style, 0, upload_bytes);
                }
            }

            if (!resources->pipeline)
            {
                const graph::GeometryVertexFormat text_gvf = graph::CreateTextGeometryVertexFormat();
                resources->pipeline = render_pass->CreatePipeline(resources->material,
                                                                  resources->binding_vil,
                                                                  graph::mtl::MakeSolid2DConfig(),
                                                                  false,
                                                                  &text_gvf);
                if (!resources->pipeline)
                    continue;
            }

            graph::TextGeometry* geometry = resources->geometry;
            if (!geometry)
            {
                const uint32_t estimate = input.total_chars;
                geometry = new graph::TextGeometry(device,
                                                   graph::CreateTextGeometryVertexFormat(),
                                                   estimate);
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
                    }
                    else
                        resources->last_draw_char_count = 0;
                }
            }

            if (!resources->data_buffer || !resources->draw_range)
            {
                SAFE_CLEAR(resources->data_buffer);
                SAFE_CLEAR(resources->draw_range);

                resources->data_buffer = new graph::GeometryDataBuffer(resources->binding_vil->GetVertexAttribCount(),
                                                                       geometry->GetIBO(),
                                                                       geometry->GetVDM());
                if (!resources->data_buffer)
                {
                    GLogError("[TextRenderPipeline] Create GeometryDataBuffer failed");
                    continue;
                }

                resources->draw_range = new graph::GeometryDrawRange();
                if (!resources->draw_range)
                {
                    SAFE_CLEAR(resources->data_buffer);
                    GLogError("[TextRenderPipeline] Create GeometryDrawRange failed");
                    continue;
                }
            }

            if (!resources->data_buffer->Update(geometry,
                                                resources->binding_vil->GetVIFList(),
                                                resources->binding_vil->GetVertexAttribCount()))
            {
                GLogError("[TextRenderPipeline] GeometryDataBuffer::Update failed");
                continue;
            }

            resources->draw_range->Set(geometry);
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
