#pragma once

#include <hgl/ecs/core/render/ShaderProgramPipelineKey.h>
#include <hgl/type/UnorderedMap.h>
#include <vector>
#include <memory>
#include <cstdint>

namespace hgl::graph
{
    struct CameraInfo;
}

namespace hgl::ecs
{
    class RenderItem;
    class MaterialBatch;

    /**
     * RenderFrameCache - Per-frame renderable collection and batch cache
     */
    struct RenderFrameCache
    {
        std::vector<std::unique_ptr<RenderItem>> renderItems;
        ::hgl::UnorderedMap<ShaderProgramPipelineKey, std::unique_ptr<MaterialBatch>> materialBatches;
        std::vector<MaterialBatch*> ordered_batches;    ///< 帧内排序复用缓冲（零逐帧堆分配）
        const graph::CameraInfo* cameraInfo = nullptr;
        uint32_t renderableCount = 0;

        /// **世界内帧序列号**：每次渲染 pass 准备时，由世界统一调用 `BeginFrame()` 自增；
        /// 不依赖任何具体的渲染系统是否启用。
        ///
        /// 与 `ECSContext::GetFrameIndex()` **不同** —— 后者是驱动相机的「帧槽号」
        /// （主帧 = 交换链图像号，要 0..N 循环；离屏 pass = 该 RT 的固定槽），
        /// 对离屏世界**永不推进**。任何以「每帧一次」为语义的守卫都必须用本序列号：
        /// 用帧槽号做键会让「首帧准备失败」被缓存到该槽的**永久**（离屏世界
        /// 首帧 renderItems 必然为空 ⇒ 准备失败被缓存 ⇒ 批次永不装配 ⇒ 离屏永不绘制）。
        uint32_t frame_serial = 0;

        // ── 阶段账本与自省状态 ──────────────────────────────────────────────
        bool has_pending_resources = false;
        bool is_collect_steady     = false;
        bool fast_path_engaged     = false;

        uint32_t scene_primitives  = 0;
        uint32_t passed_can_render = 0;
        uint32_t collected_items   = 0;

        uint32_t dropped_invisible         = 0;
        uint32_t dropped_no_transform     = 0;
        uint32_t dropped_mobility         = 0;
        uint32_t dropped_resource_pending = 0;
        uint32_t dropped_geometry_loading = 0;
        uint32_t dropped_other            = 0;
        bool drop_sample_truncated = false;

        static constexpr uint32_t kMaxSampleDrops = 16;
        struct DropSampleRecord
        {
            char entity_name[64] = {};
            uint32_t entity_id   = 0;
            char stage[32]       = {};
            char reason[32]      = {};
            char detail[64]      = {};
        };
        uint32_t drop_sample_count = 0;
        DropSampleRecord drop_samples[kMaxSampleDrops];

        void RecordDrop(const char *name, uint32_t id, const char *stage, const char *reason, const char *detail);

        RenderFrameCache() = default;
        ~RenderFrameCache();

        void BeginFrame();
    };
}
