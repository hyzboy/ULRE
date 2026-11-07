#pragma once

// 兼容层：纹理上传任务已泛化为通用 GpuUploadTask（见 GpuUploadTask.h）。
// 保留本头文件与类型别名，使既有调用代码（TextureManager / TextureStreamingSystem /
// TextureLoadCommitter 等）无需改动即可平滑编译。
#include <hgl/graph/module/GpuUploadTask.h>

namespace hgl::graph
{
    using TextureUploadTask = GpuUploadTask;
}
