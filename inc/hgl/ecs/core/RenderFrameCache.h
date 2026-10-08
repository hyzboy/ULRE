#pragma once

#include <hgl/ecs/core/ShaderProgramPipelineKey.h>
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

        RenderFrameCache() = default;
        ~RenderFrameCache();

        void BeginFrame();
    };
}
