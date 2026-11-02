#include <hgl/ecs/core/render/RenderFrameCache.h>
#include <hgl/ecs/core/render/RenderItem.h>
#include <hgl/ecs/core/render/MaterialBatch.h>

namespace hgl::ecs
{
    RenderFrameCache::~RenderFrameCache() = default;

    void RenderFrameCache::BeginFrame()
    {
        ++frame_serial;

        renderItems.clear();
        ordered_batches.clear();
        renderableCount = 0;
        cameraInfo = nullptr;

        // 清空批次内容，保留对象以供重用
        for (auto& pair : materialBatches)
        {
            if (pair.second)
                pair.second->Clear();
        }
    }
}
