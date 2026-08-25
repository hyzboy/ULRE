#include<hgl/ecs/support/TextRenderPipeline.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/graph/geo/GeometryCreater.h>   // FloatToHalf
#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/vk/VKRenderPass.h>
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
#include<algorithm>

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

            const uint32_t height = fs->IsSDFEnabled()
                ? hgl_align_pow2(fs->GetCharHeight() + 2 + 2 * graph::TEXT_SDF_SPREAD, 4)   //SDF tile 需要四周额外 spread 空间
                : hgl_align_pow2(fs->GetCharHeight() + 2, 4);

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
                            const int char_height,
                            const uint16_t style_id = 0,
                            const graph::layout::CharStyle* char_style = nullptr)
        {
            out_style.para_style = para_style;
            out_style.start_position = start_pos;
            out_style.style_id = style_id;

            const float origin_char_height = static_cast<float>(char_height);

            out_style.char_height = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height));
            out_style.space_size = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.space_size));
            out_style.full_space_size = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.full_space_size));
            out_style.tab_size = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.tab_size));
            out_style.char_gap = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.char_gap));
            out_style.line_gap = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height * para_style.line_gap));
            out_style.line_height = static_cast<graph::layout::TEXT_COORD_TYPE>(std::ceil(origin_char_height + out_style.line_gap));

            // bold/outline 使字形视觉尺寸每侧扩展 bold+outline 像素，
            // 因此水平/垂直行间距各增加 2*(bold+outline) 以避免字符重叠
            if (char_style)
            {
                const float extra = 2.0f * (char_style->bold + char_style->outline);
                out_style.extra_advance_x = extra;
                out_style.extra_advance_y = extra;
            }
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

            SAFE_CLEAR(res.mesh_draw_params);

            if (res.char_info_buffer && buffer_manager)
            {
                buffer_manager->Release(res.char_info_buffer);
                res.char_info_buffer = nullptr;
            }

            if (res.char_style_buffer && buffer_manager)
            {
                buffer_manager->Release(res.char_style_buffer);
                res.char_style_buffer = nullptr;
            }

            if (res.char_instance_buffer && buffer_manager)
            {
                buffer_manager->Release(res.char_instance_buffer);
                res.char_instance_buffer = nullptr;
            }

            if (res.material && material_manager)
            {
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

            if (res.last_draw_char_count == 0)
                continue;

            // ── GPU path: CharQuad mesh shader generates quads ──────────
            if (!res.pipeline || !res.material)
                continue;

            cmd->BindPipeline(res.pipeline);

            // Bind GPU text SSBOs (b14/b15/b16) to material PerObject set
            if (auto* mp = res.material->GetMP(graph::DescriptorSetType::PerObject))
            {
                if (res.char_info_buffer && res.char_info_buffer->GetGPUBuffer())
                    mp->BindSSBO(14, res.char_info_buffer->GetGPUBuffer());
                if (res.char_style_buffer && res.char_style_buffer->GetGPUBuffer())
                    mp->BindSSBO(15, res.char_style_buffer->GetGPUBuffer());
                if (res.char_instance_buffer && res.char_instance_buffer->GetGPUBuffer())
                    mp->BindSSBO(16, res.char_instance_buffer->GetGPUBuffer());
            }

            // Bind mesh_draw_params to material
            if (res.mesh_draw_params)
                res.material->BindSSBO(graph::DescriptorSetType::PerObject,
                                       "mesh_draw_params",
                                       res.mesh_draw_params->GetGPUBuffer()->GetVkDeviceBuffer(),
                                       0, VK_WHOLE_SIZE);

            cmd->BindDescriptorSets(res.material);

            // Scene / Bindless descriptor sets with material's pipeline layout
            if (auto* gc = render_context ? render_context->GetGraphicsContext() : nullptr)
            {
                const VkPipelineLayout layout = res.material->GetPipelineLayout();

                if (auto *scene_set = gc->GetGlobalSceneUBOSet();
                    scene_set && scene_set->IsValid())
                    scene_set->BindToCmd(*cmd, layout);

                if (auto *bindless_mgr = gc->GetBindlessTextureManager();
                    bindless_mgr && bindless_mgr->IsValid())
                    bindless_mgr->BindToCmd(*cmd, layout,
                                            static_cast<uint32_t>(graph::DescriptorSetType::Bindless));
            }

            // CharQuad dispatch: 42 chars per group (max_invocations for CharQuad)
            const uint32_t char_count = res.last_draw_char_count;
            const uint32_t group_count = (char_count + 41u) / 42u;
            cmd->DrawMeshTasks(group_count);
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
            graph::DescriptorBindingSet* descriptor_binding_set = nullptr;
            graph::BufferManager* buffer_manager = nullptr;
            graph::DeviceBuffer* texture_layer_buffer = nullptr;
            graph::DeviceBuffer* data_index_row_buffer = nullptr;
            std::unique_ptr<graph::TileFont> tile_font;
            bool committed = false;

            ~BuildGuard()
            {
                if (committed)
                    return;

                if (texture_layer_buffer && buffer_manager)
                    buffer_manager->Release(texture_layer_buffer);

                if (data_index_row_buffer && buffer_manager)
                    buffer_manager->Release(data_index_row_buffer);

                if (descriptor_binding_set)
                    delete descriptor_binding_set;

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
        // SDF 与原始位图走不同解码路径，按字体源开关选择对应材质定义，
        // 修正原"原始位图也走 SDF 解码路径"的错配。
        recipe.mtl_def_id = font_source->IsSDFEnabled()
            ? hgl::graph::mtl::BUILTIN_MTL_DEF_TEXT           //"builtin/text_gpu" SDF 距离场解码路径
            : hgl::graph::mtl::BUILTIN_MTL_DEF_TEXT_BITMAP;   //"builtin/text_gpu_bitmap" 原始位图采样路径
        recipe.render_state_overrides.pipeline_config = graph::mtl::MakeSolid2DConfig();

        material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return nullptr;

        guard.material_manager = material_manager;

        {
            graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
            mtl_request.recipe = recipe;
            mtl_request.primitive_type = graph::PrimitiveType::Triangles;
            // CharQuad mode: no geometry_vertex_format needed (self-declares SSBOs)
            guard.material = material_manager->AcquireShaderProgram(mtl_request);
        }
        if (!guard.material)
            return nullptr;

        guard.descriptor_binding_set = new graph::DescriptorBindingSet(guard.material);
        if (!guard.descriptor_binding_set)
            return nullptr;

        buffer_manager = graphics_context->GetBufferManager();
        if (!buffer_manager)
            return nullptr;

        guard.buffer_manager = buffer_manager;

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
        resources.descriptor_binding_set = guard.descriptor_binding_set;
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

            // 去重收集 CharStyle，分配 style_id
            const auto& cs = text_comp->GetCharStyle();
            uint16_t style_id = 0;
            auto it_found = std::find(input.styles.begin(), input.styles.end(), cs);
            if (it_found == input.styles.end())
            {
                style_id = static_cast<uint16_t>(input.styles.size());
                input.styles.push_back(cs);
            }
            else
            {
                style_id = static_cast<uint16_t>(std::distance(input.styles.begin(), it_found));
            }
            input.style_ids.push_back(style_id);

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
            const bool style_changed = font_changed || (resources->styles != input.styles);

            if (style_changed)
            {
                resources->styles = input.styles;
                input.dirty = true;
            }

            auto *binding_set = resources->descriptor_binding_set;
            if (!binding_set)
                continue;

            if (!resources->pipeline && resources->material)
            {
                // CharQuad mesh shader self-declares all vertex SSBOs; no geometry_vertex_format
                resources->pipeline = render_pass->CreatePipeline(resources->material,
                                                                  graph::mtl::MakeAlpha2DConfig(),
                                                                  nullptr);
                if (!resources->pipeline)
                    GLogError("[TextRenderPipeline] Failed to create GPU text pipeline");
            }

            const bool should_layout = input.dirty ||
                                       resources->last_draw_char_count == 0 ||
                                       resources->last_string_count != static_cast<uint32_t>(input.texts.size());

            if (should_layout)
            {
                graph::layout::TextLayout layout_engine(resources->tile_font);
                if (layout_engine.Begin(&resources->chars_sets, input.total_chars))
                {
                    for (size_t ti = 0; ti < input.texts.size(); ++ti)
                    {
                        const auto* text_comp = input.texts[ti];
                        const uint16_t comp_style_id = (ti < input.style_ids.size()) ? input.style_ids[ti] : 0;
                        const graph::layout::CharStyle* comp_char_style =
                            (comp_style_id < static_cast<uint16_t>(input.styles.size()))
                            ? &input.styles[comp_style_id] : nullptr;

                        graph::layout::TextDrawStyle draw_style;
                        BuildDrawStyle(draw_style,
                                       text_comp->GetParagraphStyle(),
                                       text_comp->GetStartPosition(),
                                       resources->tile_font->GetFontSource()->GetCharHeight(),
                                       comp_style_id,
                                       comp_char_style);

                        layout_engine.AddString(text_comp->GetText(), draw_style);
                    }

                    const int draw_count = layout_engine.End();
                    if (draw_count > 0)
                    {
                        resources->last_draw_char_count = static_cast<uint32_t>(draw_count);

                        // === Phase A: Build GPU three-layer data model ===
                        auto* graphics_ctx = render_context ? render_context->GetGraphicsContext() : nullptr;
                        auto* buf_mgr = graphics_ctx ? graphics_ctx->GetBufferManager() : nullptr;

                        if (buf_mgr)
                        {
                            // 1. Use char_info_table and gpu_char_instances directly from TextLayout
                            //    (TextLayout now builds the unique char table and assigns char_id during sl_l2r)
                            const auto& unique_chars = layout_engine.GetCharInfoTable();
                            const auto& gpu_instances = layout_engine.GetGpuCharInstances();

                            // 阴影 UV 偏移：阴影偏移为 0.1 倍字符高度(像素)，
                            // 换算为图集归一化坐标后打包为两个 half-float (du 低位, dv 高位)
                            uint32_t shadow_uv_offset = 0;
                            {
                                const float off_px = 0.05f * static_cast<float>(
                                    resources->tile_font->GetFontSource()->GetCharHeight());
                                auto* atlas = resources->tile_font->GetTileData()->GetTexture();

                                if (atlas && atlas->GetWidth() > 0 && atlas->GetHeight() > 0)
                                {
                                    const float du = off_px / static_cast<float>(atlas->GetWidth());
                                    const float dv = off_px / static_cast<float>(atlas->GetHeight());

                                    shadow_uv_offset = (static_cast<uint32_t>(graph::FloatToHalf(dv)) << 16)
                                                     |  static_cast<uint32_t>(graph::FloatToHalf(du));
                                }
                            }

                            // 2. Build CharStyleGPU table from all unique styles
                            std::vector<graph::layout::CharStyleGPU> styles;
                            styles.reserve(resources->styles.size() > 0 ? resources->styles.size() : 1);
                            if (resources->styles.empty())
                            {
                                // fallback: one default style，新增字段补零(阴影颜色保持默认黑)
                                graph::layout::CharStyleGPU s{};
                                s.text_color    = HGL_U8_TO_RGBA8(255, 255, 255, 255);
                                s.outline_color = 0;
                                s.shadow_color  = HGL_U8_TO_RGBA8(0, 0, 0, 255);
                                s.flags         = 0;
                                s.italic        = 0.0f;
                                s.bold_px       = 0.0f;
                                s.outline_px    = 0.0f;
                                s.shadow_uv_offset = 0;
                                styles.push_back(s);
                            }
                            else
                            {
                                for (const auto& cs : resources->styles)
                                {
                                    graph::layout::CharStyleGPU s{};
                                    const auto& c  = cs.CharColor;
                                    const auto& oc = cs.OutlineColor;
                                    const auto& sc = cs.ShadowColor;

                                    s.text_color    = HGL_U8_TO_RGBA8(c.r, c.g, c.b, c.a);
                                    s.outline_color = HGL_U8_TO_RGBA8(oc.r, oc.g, oc.b, oc.a);
                                    s.shadow_color  = HGL_U8_TO_RGBA8(sc.r, sc.g, sc.b, sc.a);
                                    s.flags         = cs.shadow ? 1u : 0u;      //bit0 = shadow_enabled
                                    s.italic        = cs.italic;
                                    s.bold_px       = cs.bold;
                                    s.outline_px    = std::min(cs.outline, static_cast<float>(graph::TEXT_SDF_SPREAD));
                                    s.shadow_uv_offset = shadow_uv_offset;
                                    styles.push_back(s);
                                }
                            }

                            // 3. Create / resize GPU SSBOs and upload data
                            const VkDeviceSize char_info_bytes   = unique_chars.size()   * sizeof(graph::layout::TextCharInfo);
                            const VkDeviceSize style_bytes     = styles.size()      * sizeof(graph::layout::CharStyleGPU);
                            const VkDeviceSize instance_bytes  = gpu_instances.size() * sizeof(graph::layout::CharInstance);

                            if (!resources->char_info_buffer || resources->char_info_buffer->GetSize() < char_info_bytes)
                            {
                                if (resources->char_info_buffer)
                                    buf_mgr->Release(resources->char_info_buffer);
                                resources->char_info_buffer = buf_mgr->CreateSSBO(
                                    "Text2D_CharInfo", char_info_bytes, graph::SharingMode::Exclusive);
                            }

                            if (!resources->char_style_buffer || resources->char_style_buffer->GetSize() < style_bytes)
                            {
                                if (resources->char_style_buffer)
                                    buf_mgr->Release(resources->char_style_buffer);
                                resources->char_style_buffer = buf_mgr->CreateSSBO(
                                    "Text2D_CharStyle", style_bytes, graph::SharingMode::Exclusive);
                            }

                            if (!resources->char_instance_buffer || resources->char_instance_buffer->GetSize() < instance_bytes)
                            {
                                if (resources->char_instance_buffer)
                                    buf_mgr->Release(resources->char_instance_buffer);
                                resources->char_instance_buffer = buf_mgr->CreateSSBO(
                                    "Text2D_CharInstance", instance_bytes, graph::SharingMode::Exclusive);
                            }

                            if (resources->char_info_buffer)
                            {
                                auto* gpu = resources->char_info_buffer->GetGPUBuffer();
                                if (gpu) gpu->Write(unique_chars.data(), 0, char_info_bytes);
                            }
                            if (resources->char_style_buffer)
                            {
                                auto* gpu = resources->char_style_buffer->GetGPUBuffer();
                                if (gpu) gpu->Write(styles.data(), 0, style_bytes);
                            }
                            if (resources->char_instance_buffer)
                            {
                                auto* gpu = resources->char_instance_buffer->GetGPUBuffer();
                                if (gpu) gpu->Write(gpu_instances.data(), 0, instance_bytes);
                            }

                            resources->unique_char_count = static_cast<uint32_t>(unique_chars.size());
                            resources->style_count       = static_cast<uint32_t>(styles.size());
                        }
                    }
                    else
                        resources->last_draw_char_count = 0;
                }
            }

            resources->last_string_count = static_cast<uint32_t>(input.texts.size());

            // IndirectMeshDraw：写 mesh per-draw 参数行 row 0（每字体一次 draw，
            // gl_DrawID=0；shader 不再读 push constant。build 阶段写入，
            // RenderBufferUploadSystem 上传后再进录制）
            if (device && resources->last_draw_char_count > 0)
            {
                if (!resources->mesh_draw_params)
                    resources->mesh_draw_params = device->CreateSSBO(
                        "ECS:Text:MeshDrawParams", sizeof(graph::mtl::MeshDrawParams));

                if (resources->mesh_draw_params)
                {
                    auto *gpu = resources->mesh_draw_params->GetGPUBuffer();
                    auto *row = gpu ? static_cast<graph::mtl::MeshDrawParams *>(
                        gpu->Map(0, sizeof(graph::mtl::MeshDrawParams))) : nullptr;

                    if (row)
                    {
                        row->index_base      = 0;
                        row->vertex_base     = 0;
                        row->first_instance  = 0;

                        // GPU path: total_vertices = character count (CharQuad reads this)
                        // viewport_height = char_height (for baseline correction in shader)
                        row->is_indexed      = 0;
                        row->total_vertices  = resources->last_draw_char_count;
                        row->viewport_height = static_cast<float>(input.font_source->GetCharHeight());
                        gpu->Unmap();
                    }
                }
            }
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
