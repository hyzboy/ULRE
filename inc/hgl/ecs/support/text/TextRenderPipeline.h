#pragma once

#include<hgl/ecs/support/pipeline/RenderPipelineBase.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/mtl/RenderLayer.h>
#include<hgl/mtl/PipelineConfig.h>
#include<hgl/vk/buffer/ArrayView.h>
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

        /**
         * 文本资源分桶键：(字体源, 渲染分段)。同一字体用于不同层级时，
         * 必须拆分出独立的 RenderResources——其 SSBO 三表 / mesh_draw_params
         * 存的是"本层级的排版输出"，若两层共享一份资源会互相覆盖。
         */
        struct TextLayerKey
        {
            graph::FontSource*      font_source = nullptr;
            graph::mtl::RenderLayer layer = graph::mtl::RenderLayer::UI2D;

            bool operator==(const TextLayerKey& o) const
            {
                return font_source == o.font_source && layer == o.layer;
            }
        };

        struct TextLayerKeyHash
        {
            size_t operator()(const TextLayerKey& k) const noexcept
            {
                return std::hash<graph::FontSource*>{}(k.font_source)
                     ^ (static_cast<size_t>(k.layer) * 0x9E3779B97F4A7C15ull);
            }
        };

        class TextRenderPipeline : public RenderPipelineBase
        {
        private:
            // W7 统一：类静态成员 kName（Line/Text/Primitive 三管线同模式）
            static const AnsiString kName;
            struct RenderResources
            {
                graph::TileFont* tile_font = nullptr;
                graph::ShaderProgram* material = nullptr;
                graph::Pipeline* pipeline = nullptr;

                /// EDS 1/2/3：管线不带渲染状态 ⇒ 建管线时的那份配置留在这里，绘制期下发
                ///（见 `inc/hgl/vk/pipeline/VKPipeline.h`；`MakeAlpha2DConfig()`：2D overlay + alpha blend）。
                graph::mtl::MaterialPipelineConfig pipeline_config{};
                graph::MaterialTextureConfigurationAllocation
                    texture_configuration;
                graph::DeviceBuffer* material_instance_addresses_buffer =
                    nullptr;
                uint64_t texture_pool_base = 0;
                graph::DeviceBuffer* mesh_draw_params = nullptr;    ///<mesh per-draw 参数表（row 0——每字体单 draw）
                uint32_t bindless_atlas_handle = 0;

                // GPU path SSBOs：L1 DeviceBuffer（本管线拥有）+ L3 数组视图（写/dirty）
                graph::DeviceBuffer* char_info_buffer = nullptr;      // b14
                graph::DeviceBuffer* char_style_buffer = nullptr;     // b15
                graph::DeviceBuffer* char_instance_buffer = nullptr;  // b16
                std::unique_ptr<graph::ArrayView<graph::layout::TextCharInfo>> char_info_view;
                std::unique_ptr<graph::ArrayView<graph::layout::CharStyle>>    char_style_view;
                std::unique_ptr<graph::ArrayView<graph::layout::CharInstance>> char_instance_view;


                std::vector<graph::layout::CharStyle> styles;   ///<收集的所有组件样式（GPU路径用）
                graph::U32CharSet chars_sets;                   ///<当前字体图集中已注册字符合集（供字库图集逐帧淘汰）

                uint32_t last_draw_char_count = 0;
                uint32_t last_string_count = 0;
            };

            struct BatchInput
            {
                graph::FontSource* font_source = nullptr;
                graph::mtl::RenderLayer render_layer = graph::mtl::RenderLayer::UI2D;
                std::vector<TextComponent*> texts;
                std::vector<graph::layout::CharStyle> styles;   ///<批次内所有组件的样式（去重后）
                std::vector<uint16_t> style_ids;                ///<与 texts 平行的 style_id 表
                uint32_t total_chars = 0;
                bool dirty = false;
                bool processed = false;                         ///<本帧布局已完整消费该批次变更（RunSync 据此清除组件脏位）
            };

            ECSContext* world = nullptr;
            graph::RenderContext* render_context = nullptr;

            std::unordered_map<TextLayerKey, RenderResources, TextLayerKeyHash> resources_by_font;

            uint32_t prepared_frame_index = std::numeric_limits<uint32_t>::max();
            ValueArray<TextComponent *> frame_texts;    ///< T2-c：池活跃集视图（裸指针）
            std::unordered_map<TextLayerKey, BatchInput, TextLayerKeyHash> frame_inputs;
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

            const AnsiString&   GetName()  const override;
            ECSContext*         GetWorld() const override;

            bool PrepareFrame() override;
            void RunCollect()   override;
            void RunBuild()     override;
            void RunSync()      override;
            void Render(graph::RenderCmdBuffer* cmd) override;

            /// 只提交指定渲染分段的文本（UI2D → TextRenderSystem，
            /// Overlay → TextOverlayRenderSystem）。虚拟 Render 默认提交 UI2D。
            void RenderLayer(graph::RenderCmdBuffer* cmd, graph::mtl::RenderLayer layer);

        private:
            RenderResources* GetOrCreateResources(const TextLayerKey& key, uint32_t estimate_chars);

            bool PrepareFrameResources(graph::GraphicsContext*& graphics_context,
                                       graph::ShaderProgramManager*& material_manager,
                                       graph::RenderPass*& render_pass,
                                       graph::VulkanDevice*& device,
                                       graph::IRenderTarget*& render_target);

            void BuildInputs(ValueArray<TextComponent *> &texts,
                             std::unordered_map<TextLayerKey, BatchInput, TextLayerKeyHash>& inputs);

            void ProcessInputs(std::unordered_map<TextLayerKey, BatchInput, TextLayerKeyHash>& inputs,
                               graph::RenderPass* render_pass,
                               graph::VulkanDevice* device);

            void ClearChanges(const ValueArray<TextComponent *> &texts);
        };
    }
}
