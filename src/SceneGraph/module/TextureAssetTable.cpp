#include<hgl/graph/module/TextureAssetTable.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/RenderTargetAsset.h>
#include<hgl/graph/asset/TextureArraySpec.h>
#include<hgl/asset/LoadedFile.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    void TextureAssetTable::ReleaseAll()
    {
        if (assets.GetCount() <= 0)
            return;

        ValueArray<TextureAsset *> to_delete;

        for (int i = 0; i < assets.GetCount(); ++i)
            if (assets[i])
                to_delete.Add(assets[i]);

        assets.Clear();

        for (int i = 0; i < to_delete.GetCount(); ++i)
        {
            to_delete[i]->Release();
            delete to_delete[i];
        }
    }

    TextureAsset *TextureAssetTable::CreateFromFilename(const OSString &filename, bool auto_mipmaps)
    {
        if (filename.IsEmpty())
        {
            GLogError(u8"[TextureAssetTable] CreateTextureAsset 收到空文件名");
            return(nullptr);
        }

        // 作者可以直接写 `rt:GBuffer:depth`：解析后落到 RenderTarget 句柄表，
        // 由 RenderTargetManager 在附件创建时绑定，完全不读盘。
        // 先做无扩展名探测：`texoct:x`/`texcube:x`/`x` 补全成带容器别名的规范名，
        // 后续 Parse、去重键、meta.source_path 全部使用补全后的文本。
        const TextureURI uri=TextureURI::Parse(ProbeTextureURI(filename));

        if (!uri.IsValid())
        {
            GLogError(OS_TEXT("[TextureAssetTable] 纹理 URI 非法：%s"), filename.c_str());
            return(nullptr);
        }

        if (uri.IsRenderTarget())
            return(GetOrCreateRenderTarget(uri.path, uri.attachment));

        if (uri.IsTextureArray())
            return(GetOrCreateTextureArray(uri));

        // 规范化键 ⇒ "res/a.Tex2D"、"res\\a.Tex2D"、"RES/./a.Tex2D" 归到同一个句柄。
        // 判等要连 source_kind 一起看：同一路径用 `file:` 和 `cubefile:` 引用是两张不同的纹理。
        const OSString key=MakeLoadedFileKey(uri.path);

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            TextureAsset *exist=assets[i];

            if (exist && exist->GetSourceKind() == uri.kind
                && exist->GetAutoMipmaps() == auto_mipmaps
                && MakeLoadedFileKey(exist->GetFilename()) == key)
            {
                // 追踪用：命中已有句柄 ⇒ 谁在引用它（配方名由调用方 MaterialRecipeRegistry 打）
                GLogVerbose(u8"[TextureAssetTable] 纹理句柄命中已有：#%d %s",
                            i, ToU8String(uri.raw).c_str());
                return(exist);
            }
        }

        TextureAsset *asset=new TextureAsset(owner);

        if (!asset->SetupFromFilename(uri.raw, auto_mipmaps))
        {
            delete asset;
            return(nullptr);
        }

        assets.Add(asset);

        // 追踪用：句柄入表 = 进入 TextureStreamingSystem 的工作集（之后会被无条件读盘）。
        // 想知道"这个文件是谁要开的"，往上看同一条 URI 的 [MaterialRecipeRegistry] 归属日志。
        GLogDebug(u8"[TextureAssetTable] 纹理句柄入表：#%d（total=%d）%s auto_mip=%d",
                  assets.GetCount()-1, assets.GetCount(),
                  ToU8String(uri.raw).c_str(), auto_mipmaps?1:0);

        return(asset);
    }

    TextureAsset *TextureAssetTable::GetOrCreateTextureArray(const TextureURI &uri)
    {
        const OSString uri_text=uri.ToString();

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            TextureAsset *exist=assets[i];

            if (exist && exist->IsTextureArray() && exist->GetTextureArrayURI() == uri_text)
                return(exist);
        }

        const OSString spec_path=ResolveTextureArraySpecPath(uri);

        if (spec_path.IsEmpty())
        {
            GLogError(OS_TEXT("[TextureAssetTable] 纹理数组 URI 无法解析出规约路径：%s"), uri.raw.c_str());
            return(nullptr);
        }

        TextureArraySpec            spec;
        TextureArraySpecParseError  spec_error;

        if (ParseTextureArraySpecFile(spec_path, spec, &spec_error) != TextureArraySpecParseResult::OK)
        {
            GLogError(OS_TEXT("[TextureAssetTable] 纹理数组规约解析失败：%s（%s）"),
                      spec_path.c_str(),
                      ToU8String(spec_error.Format()).c_str());
            return(nullptr);
        }

        TextureAsset *asset=new TextureAsset(owner);

        if (!asset->SetupFromTextureArray(spec, uri_text))
        {
            delete asset;
            return(nullptr);
        }

        assets.Add(asset);

        // 追踪用：数组句柄在**创建期**就同步读盘装配（与文件句柄的异步流送不同）
        GLogDebug(u8"[TextureAssetTable] 纹理数组句柄入表：#%d（total=%d）%s spec=%s",
                  assets.GetCount()-1, assets.GetCount(),
                  ToU8String(uri_text).c_str(), ToU8String(spec_path).c_str());

        return(asset);
    }

    TextureAsset *TextureAssetTable::FindRenderTarget(const OSString &target, const OSString &attachment) const
    {
        if (target.IsEmpty())
            return(nullptr);

        const OSString &att=attachment.IsEmpty()?OSString(kTextureURIAttachmentColor):attachment;

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            TextureAsset *exist=assets[i];

            if (!exist || !exist->IsRenderTarget())
                continue;

            if (exist->GetRenderTargetName() == target
                && exist->GetRenderTargetAttachment() == att)
                return(exist);
        }

        return(nullptr);
    }

    TextureAsset *TextureAssetTable::GetOrCreateRenderTarget(const OSString &target, const OSString &attachment)
    {
        if (target.IsEmpty())
        {
            GLogError(u8"[TextureAssetTable] GetOrCreateRenderTargetAsset 收到空 RenderTarget 名");
            return(nullptr);
        }

        const OSString &att=attachment.IsEmpty()?OSString(kTextureURIAttachmentColor):attachment;

        TextureAsset *exist=FindRenderTarget(target, att);

        if (exist)
            return(exist);

        TextureAsset *asset=new TextureAsset(owner);

        if (!asset->SetupFromRenderTarget(target, att))
        {
            delete asset;
            return(nullptr);
        }

        // 句柄可能先于 RT 创建（材质装配期引用了一个还没建的 RT）。若此刻 RT 已存在，
        // 就地取一次附件纹理把它推进 Resident，这样创建顺序不影响最终结果；
        // 反过来（RT 先建、句柄后建）由 RenderTargetManager::CreateAttachments 负责绑定。
        if (owner && owner->GetGraphicsContext())
        {
            RenderTargetManager *rtm=owner->GetGraphicsContext()->GetRenderTargetManager();

            if (rtm)
            {
                IRenderTarget *rt=rtm->Find(ToRenderTargetName(target));

                if (rt)
                {
                    Texture2D *tex=ResolveRenderTargetAttachment(rt, att);

                    if (tex)
                        asset->BindRenderTargetTexture(tex);
                }
            }
        }

        assets.Add(asset);

        return(asset);
    }

    TextureAsset *TextureAssetTable::WrapTexture(Texture *texture)
    {
        if (!texture)
        {
            GLogError(u8"[TextureAssetTable] WrapTextureAsset 收到空纹理");
            return(nullptr);
        }

        TextureAsset *asset=new TextureAsset(owner);

        if (!asset->SetupFromTexture(texture))
        {
            delete asset;
            return(nullptr);
        }

        assets.Add(asset);

        return(asset);
    }

    int TextureAssetTable::BindRenderTarget(const OSString &target, Texture *texture, const OSString &attachment)
    {
        if (target.IsEmpty() || !texture)
            return(0);

        /// 附件名只在指定时过滤；给空串表示「该目标的所有附件都用这张纹理」（仅单附件场景安全）
        const bool filter_by_attachment=!attachment.IsEmpty();

        int count=0;

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            TextureAsset *asset=assets[i];

            if (!asset || !asset->IsRenderTarget())
                continue;

            if (asset->GetRenderTargetName() != target)
                continue;

            if (filter_by_attachment && asset->GetRenderTargetAttachment() != attachment)
                continue;

            if (asset->BindRenderTargetTexture(texture))
                ++count;
        }

        return(count);
    }

    void TextureAssetTable::UnbindRenderTarget(const OSString &target)
    {
        if (target.IsEmpty())
            return;

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            TextureAsset *asset=assets[i];

            if (asset && asset->IsRenderTarget() && asset->GetRenderTargetName() == target)
                asset->UnbindRenderTargetTexture("RenderTarget 已销毁");
        }
    }

    int TextureAssetTable::MarkRenderTargetContentReady(const OSString &target)
    {
        if (target.IsEmpty())
            return(0);

        int count=0;

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            TextureAsset *asset=assets[i];

            if (!asset || !asset->IsRenderTarget())
                continue;

            if (asset->GetRenderTargetName() != target)
                continue;

            if (asset->IsContentReady())
                continue;

            asset->MarkContentReady();

            if (asset->IsContentReady())
                ++count;
        }

        return(count);
    }
}//namespace hgl::graph