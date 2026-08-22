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
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/graph/module/ShaderProgramManager.h>
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
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKGlobalSceneUBOSet.h>

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
                }

                material_manager->Release(res.material);
                res.material = nullptr;
            }

            if (res.descriptor_binding_set)
            {
                delete res.descriptor_binding_set;
                res.descriptor_binding_set = nullptr;
            }

            if (res.material_data_buffer && buffer_manager)
            {
                buffer_manager->Release(res.material_data_buffer);
                res.material_data_buffer = nullptr;
            }

            if (res.texture_layer_buffer && buffer_manager)
            {
                buffer_manager->Release(res.texture_layer_buffer);
                res.texture_layer_buffer = nullptr;
            }

            if (res.data_index_row_buffer && buffer_manager)
            {
                buffer_manager->Release(res.data_index_row_buffer);
                res.data_index_row_buffer = nullptr;
            }

            if (res.tile_font)
            {
                delete res.tile_font;
                res.tile_font = nullptr;
            }
        }
        resources_by_font.Clear();
    }

    const std::string TextRenderPipeline::kName{ "Text" };

    const std::string& TextRenderPipeline::GetName() const
    {
        return kName;
    }

    ECSContext* TextRenderPipeline::GetWorld() const
    {
        return world;
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
        if (!PrepareFrame())
            return;
        frame_inputs.clear();
        BuildInputs(frame_texts, frame_inputs);
    }

    void TextRenderPipeline::RunBuild()
    {
        if (!PrepareFrame())
            return;
        ProcessInputs(frame_inputs,
                      frame_render_pass,
                      frame_device);
    }

    void TextRenderPipeline::RunSync()
    {
        if (!PrepareFrame())
            return;
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

            // 顶点 SSBO 绑定（text_2d transport=ssbo——SSBO 顶点输入）：
            // TextGeometry 的 VAB buffer 直接绑 PerObject 集顶点槽（零复制——
            // shader 按 gl_VertexIndex 读；与 RenderDescriptorBindingSystem 的
            // 顶点 SSBO 绑定同一机制，text 不走 Primitive batch 路径故在此自绑）
            if (auto* geom = res.geometry)
            {
                if (auto* vab = geom->GetPositionVAB())
                    res.material->BindSSBO(graph::DescriptorSetType::PerObject,
                                           "VertexPosition", vab->GetGPUBuffer());
                if (auto* vab = geom->GetTexCoordVAB())
                    res.material->BindSSBO(graph::DescriptorSetType::PerObject,
                                           "VertexUV", vab->GetGPUBuffer());
                // 顶点索引 SSBO（非索引绘制——索引数据统一 SSBO）
                if (auto* ibo = geom->GetIBO())
                    res.material->BindSSBO(graph::DescriptorSetType::PerObject,
                                           "VertexIndex", ibo->GetVkBuffer(), 0, VK_WHOLE_SIZE);
            }

            // per-draw 段偏移 push constant（text 独立 VAB——段偏移恒 0，
            // 但 shader 静态使用 pc_vertex_index——必须设置；索引统一 uint32）
            const uint32_t pc_data[3] = {0, 0, 0};   // text 非索引（is_indexed=0——gl_VertexIndex 直通）
            cmd->PushConstants(res.material->GetPipelineLayout(), pc_data, sizeof(pc_data));

            cmd->BindDescriptorSets(res.material);

            // Set 0（Scene UBO）/ Set 3（Bindless 纹理）按材质自身 layout 绑定。
            // VVL 的 set 兼容 ID 取 layout 在 set 0..N 的全部 DSL 前缀，绑定 layout 必须与
            // draw 时管线 layout（= 材质 pipeline layout）一致。见 PipelineMaterialRenderer::Render。
            if (auto* gc = render_context ? render_context->GetGraphicsContext() : nullptr)
            {
                const VkPipelineLayout layout = res.material->GetPipelineLayout();

                if (auto *scene_set = gc->GetGlobalSceneUBOSet();
                    scene_set && scene_set->IsValid())
                {
                    scene_set->BindToCmd(*cmd, layout);
                }

                if (auto *bindless_mgr = gc->GetBindlessTextureManager();
                    bindless_mgr && bindless_mgr->IsValid())
                {
                    bindless_mgr->BindToCmd(*cmd,
                                            layout,
                                            static_cast<uint32_t>(graph::DescriptorSetType::Bindless));
                }
            }

            // 非索引绘制（SSBO 顶点输入——索引数据走 VertexIndex 槽，段偏移 push constant）
            cmd->Draw(res.data_buffer, res.draw_range, 1, 0);
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
        graph::BufferManager* buffer_manager = nullptr;

        struct BuildGuard
        {
            graph::ShaderProgramManager* material_manager = nullptr;
            graph::ShaderProgram* material = nullptr;
            graph::VIL* binding_vil = nullptr;
            graph::DescriptorBindingSet* descriptor_binding_set = nullptr;
            graph::BufferManager* buffer_manager = nullptr;
            graph::DeviceBuffer* material_data_buffer = nullptr;
            graph::DeviceBuffer* texture_layer_buffer = nullptr;
            graph::DeviceBuffer* data_index_row_buffer = nullptr;
            std::unique_ptr<graph::TileFont> tile_font;
            bool committed = false;

            ~BuildGuard()
            {
                if (committed)
                    return;

                if (material_data_buffer && buffer_manager)
                    buffer_manager->Release(material_data_buffer);

                if (texture_layer_buffer && buffer_manager)
                    buffer_manager->Release(texture_layer_buffer);

                if (data_index_row_buffer && buffer_manager)
                    buffer_manager->Release(data_index_row_buffer);

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
        recipe.mtl_def_id = hgl::graph::mtl::BUILTIN_MTL_DEF_TEXT;
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

        if (world)
        {
            if (auto descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>())
            {
                descriptor_binding_system->RegisterPipelineMaterial(guard.material);
            }
        }

        // 将字库图集注册进全局 bindless 纹理池，并写入 texture-layer / data-index
        // 行表第 0 行（dataIndex=0，BaseColor 槽 = 图集句柄），供 Text shader 解析。
        auto *bindless_mgr = render_context->GetManager<graph::BindlessTextureManager>();
        if (!bindless_mgr || !bindless_mgr->IsValid())
            return nullptr;

        const uint32_t atlas_handle = bindless_mgr->RegisterTexture(guard.tile_font->GetTexture());
        if (atlas_handle == 0)
            return nullptr;

        resources.bindless_atlas_handle = atlas_handle;

        auto *domain_manager = graphics_context->GetResourceDomainManager();
        if (!domain_manager)
            return nullptr;

        // mtl_texture_layer_rows：TEXTURE_SLOT_RANGE_SIZE 个 uint 一行；行 0 槽 BaseColor = atlas handle。
        constexpr uint32_t texture_layer_row_bytes =
            sizeof(uint32_t) * static_cast<uint32_t>(graph::mtl::TextureSlot::RANGE_SIZE);

        guard.texture_layer_buffer = buffer_manager->CreateSSBO(
            "Text2D_TextureLayerRows", texture_layer_row_bytes, graph::SharingMode::Exclusive);
        if (!guard.texture_layer_buffer)
            return nullptr;

        uint32_t texture_layer_row[static_cast<uint32_t>(graph::mtl::TextureSlot::RANGE_SIZE)] = {};
        texture_layer_row[0] = atlas_handle;
        guard.texture_layer_buffer->GetGPUBuffer()->Write(texture_layer_row, 0, sizeof(texture_layer_row));

        if (!guard.material->BindSSBO(graph::DescriptorSetType::Material,
                                      graph::mtl::SBS_MaterialTextureLayerRows.name,
                                      guard.texture_layer_buffer->GetGPUBuffer()))
            return nullptr;

        if (!domain_manager->RegisterBuffer(
                graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer,
                                        graph::mtl::MakeRecipeSSBOId(0), 0},
                guard.texture_layer_buffer, 1))
            return nullptr;

        resources.texture_layer_buffer = guard.texture_layer_buffer;
        guard.texture_layer_buffer = nullptr;

        // mtl_data_index_rows：MaterialDataIndexRowStride 个 uint 一行；行 0 value = 0。
        constexpr uint32_t data_index_row_bytes =
            sizeof(uint32_t) * graph::mtl::MaterialDataIndexRowStride;

        guard.data_index_row_buffer = buffer_manager->CreateSSBO(
            "Text2D_DataIndexRows", data_index_row_bytes, graph::SharingMode::Exclusive);
        if (!guard.data_index_row_buffer)
            return nullptr;

        uint32_t data_index_row[graph::mtl::MaterialDataIndexRowStride] = {};
        guard.data_index_row_buffer->GetGPUBuffer()->Write(data_index_row, 0, sizeof(data_index_row));

        if (!guard.material->BindSSBO(graph::mtl::SBS_MaterialDataIndexRows.set_type,
                                      graph::mtl::SBS_MaterialDataIndexRows.name,
                                      guard.data_index_row_buffer->GetGPUBuffer()))
            return nullptr;

        if (!domain_manager->RegisterBuffer(
                graph::mtl::SSBOAddress{graph::mtl::SSBOType::MaterialDataIndexTable,
                                        graph::mtl::MakeRecipeSSBOId(0), 0},
                guard.data_index_row_buffer, 1))
            return nullptr;

        resources.data_index_row_buffer = guard.data_index_row_buffer;
        guard.data_index_row_buffer = nullptr;

        resources.tile_font = guard.tile_font.release();
        resources.material = guard.material;
        resources.binding_vil = guard.binding_vil;
        resources.descriptor_binding_set = guard.descriptor_binding_set;
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
