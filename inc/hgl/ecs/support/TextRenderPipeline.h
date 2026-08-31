#pragma once

#include<hgl/ecs/support/RenderPipelineBase.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/vk/MirroredStructArray.h>
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
        class MaterialParameters;
        class DescriptorBindingSet;
        class Pipeline;
        class Sampler;
        class DeviceBuffer;
        class ShaderProgramManager;
        class RenderCmdBuffer;
    }

    namespace ecs
    {
        class ECSContext;
        class TextComponent;

        class TextRenderPipeline : public RenderPipelineBase
        {
        private:
            // W7 统一：类静态成员 kName（Line/Text/Primitive 三管线同模式）
            static const std::string kName;
            struct RenderResources
            {
                graph::TileFont* tile_font = nullptr;
                graph::ShaderProgram* material = nullptr;
                graph::Pipeline* pipeline = nullptr;
                graph::MaterialParameters* per_object_mp = nullptr;   ///<每字体独立 PerObject 描述符集（b14/15/16 + mesh_draw_params）
                graph::MaterialParameters* material_mp = nullptr;     ///<每字体独立 Material 描述符集（texture_layer_rows + data_index_rows）
                graph::DeviceBuffer* texture_layer_buffer = nullptr;
                graph::DeviceBuffer* data_index_row_buffer = nullptr;
                graph::DeviceBuffer* mesh_draw_params = nullptr;    ///<mesh per-draw 参数表（row 0——每字体单 draw）
                uint32_t bindless_atlas_handle = 0;

                // GPU path SSBOs (MirroredStructArray 自动管理生命周期)
                std::unique_ptr<graph::MirroredStructArray<graph::layout::TextCharInfo>> char_info_asb;    // b14
                std::unique_ptr<graph::MirroredStructArray<graph::layout::CharStyle>>    char_style_asb;   // b15
                std::unique_ptr<graph::MirroredStructArray<graph::layout::CharInstance>> char_instance_asb; // b16

                graph::DescriptorBindingSet* descriptor_binding_set = nullptr;

                std::vector<graph::layout::CharStyle> styles;   ///<收集的所有组件样式（GPU路径用）
                graph::U32CharSet chars_sets;                   ///<当前字体图集中已注册字符合集（供字库图集逐帧淘汰）

                uint32_t last_draw_char_count = 0;
                uint32_t last_string_count = 0;
            };

            struct BatchInput
            {
                graph::FontSource* font_source = nullptr;
                std::vector<const TextComponent*> texts;
                std::vector<graph::layout::CharStyle> styles;   ///<批次内所有组件的样式（去重后）
                std::vector<uint16_t> style_ids;                ///<与 texts 平行的 style_id 表
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
            ~TextRenderPipeline() override;

            void SetWorld(ECSContext* w) { world = w; }
            void SetRenderContext(graph::RenderContext* ctx) { render_context = ctx; }

            const std::string& GetName()  const override;
            ECSContext*         GetWorld() const override;

            bool PrepareFrame() override;
            void RunCollect()   override;
            void RunBuild()     override;
            void RunSync()      override;
            void Render(graph::RenderCmdBuffer* cmd) override;

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
