#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/vk/VKBufferAccessor.h>
#include <hgl/color/Color4f.h>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
        class DeviceBuffer;
        class Material;
        class MaterialInstance;
        class GraphicsPipeline;
        class RenderTargetFormat;
        class Geometry;
        class Primitive;
        class RenderCmdBuffer;
    }
    namespace ecs
    {
        class LinesComponent;
        class BoundingBoxComponent;
    }
}

namespace hgl::ecs
{
    class TransformAssignmentBuffer;
    // ----------------------------------------------------------------
    // Collect statistics (replaces LineCollectStats in old header)
    // ----------------------------------------------------------------
    struct LineCollectStats
    {
        uint32_t total_components    = 0;
        uint32_t visible_components  = 0;
        uint32_t culled_by_visibility = 0;
        uint32_t culled_by_frustum   = 0;
        uint32_t culled_by_hzb       = 0;

        float GetCullRatio() const
        {
            if (total_components == 0) return 0.0f;
            const uint32_t culled = culled_by_visibility + culled_by_frustum + culled_by_hzb;
            return static_cast<float>(culled) / static_cast<float>(total_components);
        }
    };

    // ----------------------------------------------------------------
    // LineRenderPipeline
    // Replaces LineRenderManager + LineWidthBatch + SharedLineBackup.
    // Implements RenderPipelineBase so it integrates into the unified
    // render-pipeline group architecture.
    // ----------------------------------------------------------------
    class LineRenderPipeline : public RenderPipelineBase
    {
    public:
        static constexpr uint32_t MAX_WIDTHS     = 16;
        static constexpr uint32_t PALETTE_SIZE   = 256;
        static constexpr uint32_t LINES_GRANULE  = 1024; ///< VAB capacity granularity

    private:
        // ------- Core state -------
        ECSContext*  context_           = nullptr;
        bool         initialized_       = false;
        bool         support_wide_lines_= false;
        uint32_t     prepared_frame_    = UINT32_MAX;

        // ------- GPU resources (created in Initialize()) -------
        graph::VulkanDevice*    device_         = nullptr;
        graph::Material*        material_       = nullptr;
        graph::MaterialInstance* mi_            = nullptr;
        graph::GraphicsPipeline*        pipeline_       = nullptr;
        graph::RenderTargetFormat*     render_format_  = nullptr;

        // Palette UBO (owned; buf_ is the raw GPU buffer handle)
        void*    ubo_color_   = nullptr;  ///< typed as UBOLineColorPalette* at runtime
        void*    ubo_raw_buf_ = nullptr;  ///< typed as VkBufferOwner* for release

        // CPU palette (dirty-tracked)
        uint32_t        palette_[PALETTE_SIZE] = {};
        bool            palette_dirty_         = true;

        // ------- Per-width batch slots (replace LineWidthBatch) -------
        struct LineWidthSlot
        {
            uint32_t line_count   = 0;
            uint32_t gpu_capacity = 0;   ///< current VAB capacity in line-count

            graph::Geometry*  geometry  = nullptr;
            graph::Primitive* primitive = nullptr;

            graph::BufferAccessor3f  va_pos;   ///< maps to StagedBuffer for positions
            graph::BufferAccessor1u8 va_color; ///< maps to StagedBuffer for color indices

            void Reset();
            void Clear();
            bool EnsureCapacity(uint32_t needed,
                                graph::VulkanDevice*     dev,
                                graph::MaterialInstance* mi,
                                graph::GraphicsPipeline*         p,
                                uint32_t                 width);
            bool AddSegment(const hgl::math::Vector3f& from,
                            const hgl::math::Vector3f& to,
                            uint8_t                     color_index);
            void Draw(graph::RenderCmdBuffer* cmd);
        };

        LineWidthSlot slots_[MAX_WIDTHS];

        // ------- Per-frame collect state -------
        std::vector<std::shared_ptr<LinesComponent>> collected_;
        LineCollectStats stats_;
        uint32_t total_line_count_ = 0;
        TransformAssignmentBuffer* bound_transform_buffer_ = nullptr;
        graph::DeviceBuffer* bound_transform_data_buffer_ = nullptr;

    public:
        static const std::string kName;  ///< "Line"

        explicit LineRenderPipeline(ECSContext* context);
        ~LineRenderPipeline() override;

        // ---- RenderPipelineBase interface ----
        const std::string& GetName()  const override;
        ECSContext*         GetWorld() const override;

        bool PrepareFrame()   override;
        void RunCollect()     override;
        void RunCull()        override {}   // Folded into RunCollect
        void RunSort()        override {}   // Not used for lines
        void RunBuild()       override;
        void RunSync()        override {}   // Not used for lines
        void GetRenderPrimitives(std::vector<hgl::graph::Primitive*>& out) const override {}
        void Render(hgl::graph::RenderCmdBuffer* cmd) override;
        void Shutdown()       override;

        // ---- Line-specific API ----
        /// Set a color palette entry (CPU-side; uploaded before Render)
        void SetPaletteColor(int index, const hgl::Color4f& color);

        /// Total lines written this frame (after RunBuild)
        uint32_t GetTotalLineCount() const { return total_line_count_; }

        /// Collect phase statistics (after RunCollect)
        const LineCollectStats& GetCollectStats() const { return stats_; }

    private:
        bool Initialize();
        bool ResolvePipelineForCurrentRenderTarget();
        uint32_t GetSlotIndex(uint8_t width) const;
        void FlushPaletteToGPU();
        void SyncTransformBinding();
    };

}  // namespace hgl::ecs
