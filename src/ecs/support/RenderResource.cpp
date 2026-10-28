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

        // 带文件名的句柄用路径做身份：就绪前（MetaOnly，尚无 Texture）与就绪后都能给出同一个值，
        // 因此材质配方里的绑定不需要在异步加载完成后重写；同一文件的两个句柄也自然归并。
        const hgl::OSString &filename = asset->GetFilename();

        if (!filename.IsEmpty())
        {
            const U8String u8_filename = ToU8String(filename);

            return "texfile:" + std::string(
                reinterpret_cast<const char *>(u8_filename.c_str()),
                u8_filename.Length());
        }

        // 无文件名的句柄只是「包装一张现成纹理」⇒ 身份退回纹理 ID，与句柄化之前完全一致。
        return BuildTextureResourceId(asset->GetTexture());
    }
}
