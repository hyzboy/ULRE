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

        // RT 句柄的身份 = 「目标名 + 附件名」，不能用纹理句柄兜底：
        //   - 附件名必须参与：同一 RT 的 color0 与 depth 是两张完全不同的纹理，
        //     只按目标名做键会把它们归并成同一个资源。
        //   - 也不能退回 `texid:<GetID()>`：RT 重建（改尺寸/改格式）后纹理会换新，
        //     ID 随之变化，材质配方里已记录的绑定就失效了；而「目标名 + 附件名」
        //     跨重建不变。
        if (asset->IsRenderTarget())
        {
            const U8String u8_target = ToU8String(asset->GetRenderTargetName());
            const U8String u8_att    = ToU8String(asset->GetRenderTargetAttachment());

            std::string id = "rtex:";

            id.append(reinterpret_cast<const char *>(u8_target.c_str()), u8_target.Length());
            id.append(":", 1);
            id.append(reinterpret_cast<const char *>(u8_att.c_str()), u8_att.Length());

            return id;
        }

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
