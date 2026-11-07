#pragma once

// 兼容别名：纹理上传队列已泛化为通用 GpuUploadQueue（BufferToImage / BufferToBuffer）。
// 旧代码可继续按 TextureUploadQueue / TextureUploadTask 名称引用，新代码请直接使用 Gpu* 名称。
#include <hgl/graph/module/GpuUploadQueue.h>

namespace hgl::graph
{
    using TextureUploadQueue = GpuUploadQueue;
}
