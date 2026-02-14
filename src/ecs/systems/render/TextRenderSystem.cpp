#include<hgl/ecs/systems/render/TextRenderSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextGeometry.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/type/String.h>
#include<hgl/type/MemoryUtil.h>
#include<cmath>

namespace hgl::ecs
{
    namespace
    {
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

    TextRenderSystem::TextRenderSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect, ExecutionPriority::First);
    }

    TextRenderSystem::~TextRenderSystem()
    {
        auto* device = framework ? framework->GetDevice() : nullptr;
        auto* primitive_manager = framework ? framework->GetPrimitiveManager() : nullptr;

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

            if (res.tile_font)
            {
                delete res.tile_font;
                res.tile_font = nullptr;
            }
        }
        resources_by_font.Clear();
    }

    TextRenderSystem::RenderResources* TextRenderSystem::GetOrCreateResources(graph::FontSource* font_source,
                                                                              uint32_t estimate_chars)
    {
        if (!font_source || !framework)
            return nullptr;

        if (auto* entry = resources_by_font.GetValuePointer(font_source))
            return entry;

        RenderResources resources;

        const int limit_count = static_cast<int>((estimate_chars > 0) ? estimate_chars : 256);
        resources.tile_font = framework->CreateTileFont(font_source, limit_count);
        if (!resources.tile_font)
            return nullptr;

        graph::mtl::Text2DMaterialCreateConfig mtl_cfg;
        graph::mtl::MaterialCreateInfo* mci = graph::mtl::CreateText2D(framework->GetDevAttr(), &mtl_cfg);
        if (!mci)
            return nullptr;

        auto* material_manager = framework->GetMaterialManager();
        if (!material_manager)
            return nullptr;

        static uint32_t material_id = 0;
        AnsiString material_name;
        hgl::Sprintf(material_name, "Text2D_ECS_%u", material_id++);
        resources.material = material_manager->CreateMaterial(material_name, mci);
        if (!resources.material)
            return nullptr;

        auto* sampler_manager = framework->GetSamplerManager();
        if (!sampler_manager)
            return nullptr;

        resources.sampler = sampler_manager->CreateSampler();
        if (!resources.sampler)
            return nullptr;

        if (!resources.material->BindTextureSampler(graph::DescriptorSetType::PerMaterial,
                                                    graph::mtl::SamplerName::Text,
                                                    resources.tile_font->GetTexture(),
                                                    resources.sampler))
            return nullptr;

        resources_by_font.Add(font_source, resources);
        return resources_by_font.GetValuePointer(font_source);
    }

    void TextRenderSystem::GetRenderPrimitives(std::vector<graph::Primitive*>& out_primitives) const
    {
        for (const auto& pair : resources_by_font)
        {
            if (pair.second.primitive)
                out_primitives.push_back(pair.second.primitive);
        }
    }

    void TextRenderSystem::Update(float /*deltaTime*/)
    {
        if (!world || !framework)
            return;

        auto* device = framework->GetDevice();
        if (!device)
            return;

        auto* material_manager = framework->GetMaterialManager();
        auto* primitive_manager = framework->GetPrimitiveManager();
        auto* render_pass = framework->GetDefaultRenderPass();

        if (!material_manager || !primitive_manager || !render_pass)
            return;

        struct BatchInput
        {
            graph::FontSource* font_source = nullptr;
            std::vector<const TextComponent*> texts;
            graph::layout::CharStyle batch_style{};
            uint32_t total_chars = 0;
            bool dirty = false;
        };

        std::vector<std::shared_ptr<TextComponent>> texts;
        world->GetComponents<TextComponent>(texts);

        std::unordered_map<graph::FontSource*, BatchInput> inputs;

        for (const auto& text_comp : texts)
        {
            if (!text_comp || text_comp->GetText().IsEmpty())
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

            graph::MaterialInstance* mi = resources->material_instance;
            if (!mi)
            {
                graph::VILConfig vil_config;

                vil_config.Add("Position", VF_V4I16);

                mi = material_manager->CreateMaterialInstance(resources->material,
                                                             &vil_config,
                                                             &resources->char_style,
                                                             sizeof(graph::layout::CharStyle));
                if (!mi)
                    continue;

                resources->material_instance = mi;
            }
            else if (input.dirty)
            {
                mi->WriteMIData(resources->char_style);
            }

            if (!resources->pipeline)
            {
                resources->pipeline = render_pass->CreatePipeline(mi, graph::InlinePipeline::Solid2D);
                if (!resources->pipeline)
                    continue;
            }

            graph::TextGeometry* geometry = resources->geometry;
            if (!geometry)
            {
                const uint32_t estimate = input.total_chars;
                geometry = new graph::TextGeometry(device, mi->GetVIL(), estimate);
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

        for (const auto& text_comp : texts)
        {
            if (text_comp)
                text_comp->ClearAllChanges();
        }
    }
}//namespace hgl::ecs

