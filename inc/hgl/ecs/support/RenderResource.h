#pragma once

#include <hgl/CoreType.h>
#include <string>

namespace hgl::graph
{
    class Texture;
}

namespace hgl::ecs
{
    /// 纹理资源的稳定标识（W7 上提：PrimitiveComponent / RenderPrimitiveCollectSystem /
    /// RenderDescriptorBindingSystem 三处逐字复制收敛为单一定义——实现见
    /// RenderResource.cpp，Texture 完整类型依赖留在 .cpp）
    std::string BuildTextureResourceId(const graph::Texture *texture);
}
