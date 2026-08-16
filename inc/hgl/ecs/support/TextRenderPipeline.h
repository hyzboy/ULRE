#pragma once

#include<hgl/type/UnorderedMap.h>
#include<hgl/graph/font/TextLayout.h>
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
        class RenderPass;
        class IRenderTarget;
        class VulkanDevice;
        class FontSource;
        class TileFont;
        class ShaderProgram;
        class TextGeometry;
        class DescriptorBindingSet;
        class VertexInputLayout;
        class Pipeline;
        class Sampler;
        class DeviceBuffer;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
        class ShaderProgramManager;
        class RenderCmdBuffer;
    }

    namespace ecs
    {
        class ECSContext;
        class TextComponent;

        class TextRenderPipeline
        {
        private:
            struct RenderResources
            {
                graph::TileFont* tile_font = nullptr;
                graph::ShaderProgram* material = nullptr;
                graph::Pipeline* pipeline = nullptr;
                graph::DeviceBuffer* material_data_buffer = nullptr;
                graph::DeviceBuffer* texture_layer_buffer = nullptr;
                graph::DeviceBuffer* data_index_row_buffer = nullptr;
                uint32_t bindless_atlas_handle = 0;

                graph::layout::CharStyle char_style{};
                graph::TextGeometry* geometry = nullptr;
                graph::GeometryDataBuffer* data_buffer = nullptr;
                graph::GeometryDrawRange* draw_range = nullptr;
                graph::VertexInputLayout* binding_vil = nullptr;
                graph::DescriptorBindingSet* descriptor_binding_set = nullptr;

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
            graph::ShaderProgramManager* frame_material_manager = nullptr;
            graph::RenderPass* frame_render_pass = nullptr;
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
            void Render(graph::RenderCmdBuffer* cmd);

        private:
            RenderResources* GetOrCreateResources(graph::FontSource* font_source, uint32_t estimate_chars);

            bool PrepareFrameResources(graph::GraphicsContext*& graphics_context,
                                       graph::ShaderProgramManager*& material_manager,
                                       graph::RenderPass*& render_pass,
                                       graph::VulkanDevice*& device,
                                       graph::IRenderTarget*& render_target);

            void BuildInputs(std::vector<std::shared_ptr<TextComponent>>& texts,
                             std::unordered_map<graph::FontSource*, BatchInput>& inputs);

            void ProcessInputs(std::unordered_map<graph::FontSource*, BatchInput>& inputs,
                               graph::RenderPass* render_pass,
                               graph::VulkanDevice* device);

            void ClearChanges(const std::vector<std::shared_ptr<TextComponent>>& texts);
        };
    }
}
