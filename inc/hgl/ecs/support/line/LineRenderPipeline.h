#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/vk/buffer/TypedArrayView.h>
#include <hgl/mtl/PipelineConfig.h>
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
        class ShaderProgram;
        class Pipeline;
        class Geometry;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
        class RenderCmdBuffer;
    }
    namespace ecs
    {
        class LinesComponent;
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
        /// 批 1.3 C2：`lines` 为空的组件（压根没有线段可收）——它**不是可见性剔除**，
        /// 单列一项，避免污染真视锥/可见性统计（原实现把它计进 `culled_by_visibility`）。
        uint32_t skipped_empty       = 0;
        // 2026-10-10（审计 §1.1）：`culled_by_hzb`（从不自增、恒 0）已删除；求和与三处日志同步去掉该项。

        float GetCullRatio() const
        {
            if (total_components == 0) return 0.0f;
            const uint32_t culled = culled_by_visibility + culled_by_frustum;
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

        /// EDS 1/2/3：管线不带渲染状态 ⇒ 建管线时的那份配置留在这里，绘制期下发
        ///（见 `inc/hgl/vk/pipeline/VKPipeline.h`；`MakeLineMeshConfig()`：双面 + 深度 test/write）。
        graph::mtl::MaterialPipelineConfig pipeline_config_{};

        // ------- Line buffer for one render-target frame slot -------
        struct LineBuffer
        {
            using TransformIDAccessor = hgl::graph::TypedArrayView<hgl::graph::RawDataAccess<uint32_t>>;
            using SizeAccessor        = hgl::graph::TypedArrayView<hgl::graph::RawDataAccess<hgl::math::Vector2f>>;

            uint32_t line_count   = 0;
            uint32_t gpu_capacity = 0;   ///< current VAB capacity in line-count
            uint32_t failed_capacity = 0;
            uint32_t retry_delay_frames = 0;
            uint32_t last_failure_frame = 0;

            graph::Geometry*  geometry  = nullptr;
            graph::GeometryDataBuffer* data_buffer = nullptr;
            graph::GeometryDrawRange*  draw_range  = nullptr;
            graph::ShaderProgram* material  = nullptr;   ///<SSBO 绑定用材质（LineRenderPipeline 设置）
            graph::DeviceBuffer* mesh_draw_params = nullptr;  ///<mesh per-draw 参数表（row 0——单 draw 非实例化）

            graph::TypedArrayView3f  va_pos;       ///< maps to StagedBuffer for positions
            graph::TypedArrayView1u8 va_color;     ///< maps to StagedBuffer for color indices
            TransformIDAccessor      va_transform; ///< per-vertex TransformID stream
            SizeAccessor             va_width;     ///< per-vertex width stream（Size 语义 R32_FLOAT）

            void Reset();
            void Clear();
            bool EnsureCapacity(uint32_t needed,
                                graph::VulkanDevice*     dev,
                                uint32_t                 frame_serial,
                                uint32_t                 frame_slot);
            bool AddSegment(const hgl::math::Vector3f& from,
                            const hgl::math::Vector3f& to,
                            uint8_t                     color_index,
                            float                       width,
                            float                       min_width,
                            uint32_t                    transform_index);
            void Draw(graph::RenderCmdBuffer* cmd);
        };

        // A slot may be reused only after its render target has waited for its previous submission.
        LineBuffer line_buffers_[HGL_FRAME_SLOT_TOTAL];
        LineBuffer *current_line_buffer_ = nullptr;

        // ------- Per-frame collect state -------
        std::vector<LinesComponent *> collected_;   ///< 本帧收集（池活跃集视图的裸指针；不跨帧缓存对象）
        LineCollectStats stats_;
        uint32_t total_line_count_ = 0;
        TransformAssignmentBuffer* bound_transform_buffer_ = nullptr;
        graph::DeviceBuffer* bound_transform_data_buffer_ = nullptr;

    public:
        static const AnsiString kName;  ///< "Line"

        explicit LineRenderPipeline(ECSContext* context);
        ~LineRenderPipeline() override;

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
        const LineCollectStats& GetCollectStats() const { return stats_; }

    private:
        bool Initialize();
        void SyncTransformBinding();
    };

}  // namespace hgl::ecs
