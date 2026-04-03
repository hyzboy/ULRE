#pragma once

#include<hgl/type/UnorderedMap.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/log/Log.h>
#include<cstdint>
#include<unordered_map>
#include<vector>
#include<memory>
#include<limits>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class GraphicsContext;
        class RenderTargetFormat;
        class IRenderTarget;
        class VulkanDevice;
        class FontSource;
        class TileFont;
        class Material;
        class Primitive;
        class TextGeometry;
        class MaterialInstance;
        class GraphicsPipeline;
        class Sampler;
        class DeviceBuffer;
        class MaterialManager;
        class PrimitiveManager;
    }

    namespace ecs
    {
        class ECSContext;
        class TextComponent;

        class TextRenderPipeline
        {
            OBJECT_LOGGER

        private:
            struct RenderResources
            {
                graph::TileFont* tile_font = nullptr;
                graph::Material* material = nullptr;
                graph::GraphicsPipeline* pipeline = nullptr;
                graph::Sampler* sampler = nullptr;
                graph::DeviceBuffer* material_instance_buffer = nullptr;

                graph::layout::CharStyle char_style{};
                graph::TextGeometry* geometry = nullptr;
                graph::Primitive* primitive = nullptr;
                graph::MaterialInstance* material_instance = nullptr;

                uint32_t last_draw_char_count = 0;
                uint32_t last_string_count = 0;
            };

            struct BatchInput
            {
                graph::FontSource* font_source = nullptr;
                std::vector<const TextComponent*> texts;
                graph::layout::CharStyle batch_style{};
                uint32_t total_chars = 0;
                bool dirty = false;
            };

            ECSContext* world = nullptr;
            graph::RenderContext* render_context = nullptr;

            hgl::UnorderedMap<graph::FontSource*, RenderResources> resources_by_font;

            uint32_t prepared_frame_index = std::numeric_limits<uint32_t>::max();
            std::vector<std::shared_ptr<TextComponent>> frame_texts;
            std::unordered_map<graph::FontSource*, BatchInput> frame_inputs;
            graph::GraphicsContext* frame_graphics_context = nullptr;
            graph::MaterialManager* frame_material_manager = nullptr;
            graph::PrimitiveManager* frame_primitive_manager = nullptr;
            graph::RenderTargetFormat* frame_render_pass = nullptr;
            graph::VulkanDevice* frame_device = nullptr;
            graph::IRenderTarget* frame_render_target = nullptr;

        public:
            TextRenderPipeline() = default;
            ~TextRenderPipeline();

            void SetWorld(ECSContext* w) { world = w; }
            void SetRenderContext(graph::RenderContext* ctx) { render_context = ctx; }

            bool PrepareFrame();
            void RunCollect();
            void RunBuild();
            void RunSync();

            void GetRenderPrimitives(std::vector<graph::Primitive*>& out_primitives) const;

        private:
            RenderResources* GetOrCreateResources(graph::FontSource* font_source, uint32_t estimate_chars);

            bool PrepareFrameResources(graph::GraphicsContext*& graphics_context,
                                       graph::MaterialManager*& material_manager,
                                       graph::PrimitiveManager*& primitive_manager,
                                       graph::RenderTargetFormat*& render_pass,
                                       graph::VulkanDevice*& device,
                                       graph::IRenderTarget*& render_target);

            void BuildInputs(std::vector<std::shared_ptr<TextComponent>>& texts,
                             std::unordered_map<graph::FontSource*, BatchInput>& inputs);

            void ProcessInputs(std::unordered_map<graph::FontSource*, BatchInput>& inputs,
                               graph::MaterialManager* material_manager,
                               graph::PrimitiveManager* primitive_manager,
                               graph::RenderTargetFormat* render_pass,
                               graph::VulkanDevice* device);

            void ClearChanges(const std::vector<std::shared_ptr<TextComponent>>& texts);
        };
    }
}
