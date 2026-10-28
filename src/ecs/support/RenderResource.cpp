#include<hgl/ecs/support/RenderResource.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/utf.h>

namespace hgl::ecs
{
    std::string BuildTextureResourceId(const graph::Texture *texture)
    {
        if (!texture)
            return {};

        return "texid:" + std::to_string(texture->GetID());
    }

    std::string BuildTextureResourceId(const graph::TextureAsset *asset)
    {
        if (!asset)
            return {};

        // 身份规则唯一实现在 TextureAsset::GetResourceId()：hgl::graph::mtl 的配方绑定也要用它，
        // 而 mtl 不能反向依赖 ecs，所以这里只做 U8 → std::string 的搬运 + 包装纹理兜底。
        const U8String resource_id = asset->GetResourceId();

        if (!resource_id.IsEmpty())
            return std::string(reinterpret_cast<const char *>(resource_id.c_str()), resource_id.Length());

        // 无文件名的句柄只是「包装一张现成纹理」⇒ 身份退回纹理 ID，与句柄化之前完全一致。
        return BuildTextureResourceId(asset->GetTexture());
    }
}
