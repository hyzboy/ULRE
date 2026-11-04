#pragma once

#include <hgl/ecs/support/pipeline/RenderPipelineBase.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/vk/buffer/TypedArrayView.h>
#include <hgl/mtl/PipelineConfig.h>
#include <hgl/type/ValueArray.h>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
        class DeviceBuffer;
        class ShaderProgram;
        class Pipeline;
        class Geometry;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
        class RenderCmdBuffer;
    }
    namespace ecs
    {
        class SimpleLinesComponent;
    }
}

namespace hgl::ecs
{
    class TransformAssignmentBuffer;

    // ----------------------------------------------------------------
    // Collect statistics（与 LineCollectStats 同形：线段收集/剔除账目）
    // ----------------------------------------------------------------
    struct SimpleLineCollectStats
    {
        uint32_t total_components    = 0;
        uint32_t visible_components  = 0;
        uint32_t culled_by_visibility = 0;
        uint32_t culled_by_frustum   = 0;
        /// `lines` 为空的组件（压根没有线段可收）——不是可见性剔除，单列一项。
        uint32_t skipped_empty       = 0;

        float GetCullRatio() const
        {
            if (total_components == 0) return 0.0f;
            const uint32_t culled = culled_by_visibility + culled_by_frustum;
            return static_cast<float>(culled) / static_cast<float>(total_components);
        }
    };

    // ----------------------------------------------------------------
    // SimpleLineRenderPipeline
    // 标准 Vulkan Line 绘制栈：LineList mesh 发射（layout(lines) 原生线图元，
    // 恒 1px，无宽度）。与 LineRenderPipeline（LineQuad 屏幕空间加粗）平行共存：
    // 组件为 SimpleLinesComponent，领域为 VertexPaletteLines，顶点流无 Size。
    // ----------------------------------------------------------------
    class SimpleLineRenderPipeline : public RenderPipelineBase
    {
    public:
        static constexpr uint32_t LINES_GRANULE  = 1024; ///< VAB capacity granularity
        static constexpr uint32_t MESH_GROUP_SIZE = 64;  ///< mesh shader threadgroup（每线程 1 线段）

    private:
        // ------- Core state -------
        ECSContext*  context_           = nullptr;
        bool         initialized_       = false;
        uint32_t     prepared_frame_    = UINT32_MAX;

        // ------- GPU resources (created in Initialize()) -------
        graph::VulkanDevice*    device_         = nullptr;
        graph::ShaderProgram*        material_       = nullptr;
        graph::Pipeline*        pipeline_       = nullptr;

        /// 建管线时的那份配置留在这里，绘制期下发（同 Line：双面 + 深度 test/write）。
        graph::mtl::MaterialPipelineConfig pipeline_config_{};

        // ------- Line buffer for one render-target frame slot -------
        struct LineBuffer
        {
            using TransformIDAccessor = hgl::graph::TypedArrayView<hgl::graph::RawDataAccess<uint32_t>>;

            uint32_t line_count   = 0;
            uint32_t gpu_capacity = 0;   ///< current VAB capacity in line-count
            uint32_t failed_capacity = 0;
            uint32_t retry_delay_frames = 0;
            uint32_t last_failure_frame = 0;

            graph::Geometry*  geometry  = nullptr;
            graph::GeometryDataBuffer* data_buffer = nullptr;
            graph::GeometryDrawRange*  draw_range  = nullptr;
            graph::ShaderProgram* material  = nullptr;   ///<SSBO 绑定用材质（SimpleLineRenderPipeline 设置）
            graph::DeviceBuffer* mesh_draw_params = nullptr;  ///<mesh per-draw 参数表（row 0——单 draw 非实例化）

            graph::TypedArrayView3f  va_pos;       ///< maps to StagedBuffer for positions
            graph::TypedArrayView1u8 va_color;     ///< maps to StagedBuffer for color indices
            TransformIDAccessor      va_transform; ///< per-vertex TransformID stream
            /// 注意：**没有宽度流**——LineList 恒 1px，顶点 ABI 无 Size 语义。

            void Reset();
            void Clear();
            bool EnsureCapacity(uint32_t needed,
                                graph::VulkanDevice*     dev,
                                uint32_t                 frame_serial,
                                uint32_t                 frame_slot);
            bool AddSegment(const hgl::math::Vector3f& from,
                            const hgl::math::Vector3f& to,
                            uint8_t                     color_index,
                            uint32_t                    transform_index);
            void Draw(graph::RenderCmdBuffer* cmd);
        };

        // A slot may be reused only after its render target has waited for its previous submission.
        LineBuffer line_buffers_[HGL_FRAME_SLOT_TOTAL];
        LineBuffer *current_line_buffer_ = nullptr;

        // ------- Per-frame collect state -------
        ValueArray<SimpleLinesComponent *> collected_;   ///< 本帧收集（池活跃集视图的裸指针；不跨帧缓存对象）
        SimpleLineCollectStats stats_;
        uint32_t total_line_count_ = 0;
        TransformAssignmentBuffer* bound_transform_buffer_ = nullptr;
        graph::DeviceBuffer* bound_transform_data_buffer_ = nullptr;

    public:
        static const AnsiString kName;  ///< "SimpleLine"

        explicit SimpleLineRenderPipeline(ECSContext* context);
        ~SimpleLineRenderPipeline() override;

        // ---- RenderPipelineBase interface ----
        const AnsiString&   GetName()  const override;
        ECSContext*         GetWorld() const override;

        bool PrepareFrame()   override;
        void RunCollect()     override;
        void RunCull()        override {}   // Folded into RunCollect
        void RunSort()        override {}   // Not used for lines
        void RunBuild()       override;
        void RunSync()        override {}   // Not used for lines
        void Render(hgl::graph::RenderCmdBuffer* cmd) override;
        void Shutdown()       override;

        // ---- Line-specific API ----
        /// Total lines written this frame (after RunBuild)
        uint32_t GetTotalLineCount() const { return total_line_count_; }

        /// Collect phase statistics (after RunCollect)
        const SimpleLineCollectStats& GetCollectStats() const { return stats_; }

    private:
        bool Initialize();
        void SyncTransformBinding();
    };

}  // namespace hgl::ecs
