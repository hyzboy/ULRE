#pragma once

#include <hgl/CoreType.h>
#include <string>

namespace hgl::graph
{
    class Texture;
    class TextureAsset;
}

namespace hgl::ecs
{
    /// 纹理资源的稳定标识（W7 上提：图元渲染路径 / RenderPrimitiveCollectSystem /
    /// RenderSceneUBOSystem 三处逐字复制收敛为单一定义——实现见
    /// RenderResource.cpp，Texture 完整类型依赖留在 .cpp）
    std::string BuildTextureResourceId(const graph::Texture *texture);

    /// 纹理句柄的稳定标识（统一入口）。
    /// - 句柄带文件名 ⇒ `texfile:<path>`：**就绪前后不变**（材质配方记录过的绑定不需要重写，
    ///   同一文件的两个句柄也归并到同一 bindless 资源）。
    /// - 句柄只是包装一张现成纹理（无文件名）⇒ 退化为 `BuildTextureResourceId(GetTexture())`
    ///   即 `texid:<n>`，与句柄化之前的身份完全一致。
    std::string BuildTextureResourceId(const graph::TextureAsset *asset);
}
