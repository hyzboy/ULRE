#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    TextureAsset::TextureAsset(TextureManager *tm)
        :tex_manager(tm)
    {
        // 句柄由 TextureManager 的句柄表拥有（裸指针 + 显式 delete），不走引用计数销毁路径。
        // 保留 MetaObject 默认的 KeepMetaUnloadPayload：即便有人误用 RefPtr，归零也只会卸载
        // 负载，不会把表里的指针变成悬垂。
        SetRefZeroPolicy(RefZeroPolicy::KeepMetaUnloadPayload);
    }

    bool TextureAsset::OnInitMeta()
    {
        return(true);
    }

    void TextureAsset::OnReleaseMeta()
    {
        texture         = nullptr;
        content_ready   =false;
        async_requested =false;
        async_request_id=0;
    }

    bool TextureAsset::OnLoadPayload()
    {
        if(texture)
            return(true);

        if(source_kind!=TextureURIKind::File)
        {
            // RenderTarget 来源没有「磁盘负载」可读，只能等 RenderTargetManager 绑定。
            GLogError(OS_TEXT("[TextureAsset] 非文件来源的句柄不支持同步加载（rt:%s:%s）"),
                      rt_target.c_str(),
                      rt_attachment.c_str());
            return(false);
        }

        if(!tex_manager)
        {
            GLogError(OS_TEXT("[TextureAsset] 同步加载失败：未绑定 TextureManager（%s）"),
                      GetMeta().source_path.c_str());
            return(false);
        }

        texture=tex_manager->LoadTexture2D(meta.source_path,auto_mipmaps);

        if(!texture)
        {
            GLogError(OS_TEXT("[TextureAsset] 纹理加载失败：%s"),meta.source_path.c_str());
            return(false);
        }

        return(true);
    }

    void TextureAsset::OnUnloadPayload()
    {
        // 纹理对象归 TextureManager 的 texture_set 所有（本引擎目前不销毁纹理），
        // 句柄只放弃观察权，不做 Release —— 否则会同时抹掉按文件名索引的缓存。
        texture         = nullptr;
        async_requested = false;
        async_request_id= 0;
    }

    bool TextureAsset::SetupFromFilename(const OSString &filename,bool auto_mipmaps)
    {
        if(filename.IsEmpty())
        {
            GLogError(u8"[TextureAsset] SetupFromFilename 收到空文件名");
            return(false);
        }

        if(!tex_manager)
        {
            GLogError(OS_TEXT("[TextureAsset] SetupFromFilename 时未绑定 TextureManager（%s）"),
                      filename.c_str());
            return(false);
        }

        // 作者可以直接写 `rt:GBuffer:depth`，因此这里统一走 URI 解析。
        const TextureURI uri=TextureURI::Parse(filename);

        if(!uri.IsValid())
        {
            GLogError(OS_TEXT("[TextureAsset] 纹理 URI 非法：%s"),filename.c_str());
            return(false);
        }

        if(uri.IsRenderTarget())
            return SetupFromRenderTarget(uri.path,uri.attachment);

        source_kind         =TextureURIKind::File;
        meta.source_path    =uri.path;
        meta.asset_type     =OS_TEXT("Texture2D");
        this->auto_mipmaps  =auto_mipmaps;

        return(Init());
    }

    bool TextureAsset::SetupFromRenderTarget(const OSString &target,const OSString &attachment)
    {
        if(target.IsEmpty())
        {
            GLogError(u8"[TextureAsset] SetupFromRenderTarget 收到空 RenderTarget 名");
            return(false);
        }

        if(!Init())
            return(false);

        source_kind     =TextureURIKind::RenderTarget;
        rt_target       =target;
        rt_attachment   =attachment.IsEmpty()?OSString(kTextureURIAttachmentColor):attachment;

        // source_path 保留目标名，GetFilename()/日志仍然可读，但不会进入文件名去重表。
        meta.source_path=target;
        meta.asset_type =OS_TEXT("RenderTargetTexture");

        return(true);
    }

    bool TextureAsset::SetupFromTexture(Texture *tex)
    {
        if(!tex)
        {
            GLogError(u8"[TextureAsset] SetupFromTexture 收到空纹理");
            return(false);
        }

        if(!Init())
            return(false);

        texture      =tex;
        content_ready=true;
        payload_state=PayloadState::Resident;

        return(true);
    }

    void TextureAsset::MarkAsyncRequested(const uint64 request_id)
    {
        async_requested =true;
        async_request_id=request_id;

        if(payload_state==PayloadState::MetaOnly
         ||payload_state==PayloadState::LoadFailed)
            payload_state=PayloadState::Loading;
    }

    void TextureAsset::ClearAsyncRequest()
    {
        async_requested =false;
        async_request_id=0;

        if(payload_state==PayloadState::Loading)
            payload_state=PayloadState::MetaOnly;
    }

    bool TextureAsset::CommitAsyncTexture(Texture *tex)
    {
        if(!tex)
        {
            FailAsyncLoad("CommitAsyncTexture 收到空纹理");
            return(false);
        }

        texture         =tex;
        content_ready   =true;
        async_requested =false;
        async_request_id=0;
        payload_state   =PayloadState::Resident;

        return(true);
    }

    void TextureAsset::FailAsyncLoad(const char *reason)
    {
        // source_path 是 OSString（Windows 上是 UTF-16），而 reason 是 UTF-8 —— 统一转成
        // UTF-8 再用 %s，否则格式化参数与格式串编码不一致，日志会输出乱码甚至读越界。
        const U8String u8_path=ToU8String(meta.source_path);

        GLogError(u8"[TextureAsset] 异步加载失败：%s（%s）",
                  reinterpret_cast<const char *>(u8_path.c_str()),
                  reason?reason:"未指明原因");

        texture         =nullptr;
        content_ready   =false;
        async_requested =false;
        async_request_id=0;
        payload_state   =PayloadState::LoadFailed;
    }

    bool TextureAsset::BindRenderTargetTexture(Texture *tex)
    {
        if(source_kind!=TextureURIKind::RenderTarget)
        {
            GLogError(OS_TEXT("[TextureAsset] BindRenderTargetTexture 用在非 RT 句柄上（%s）"),
                      meta.source_path.c_str());
            return(false);
        }

        if(!tex)
        {
            GLogError(OS_TEXT("[TextureAsset] BindRenderTargetTexture 收到空纹理（rt:%s:%s）"),
                      rt_target.c_str(),
                      rt_attachment.c_str());
            payload_state=PayloadState::LoadFailed;
            return(false);
        }

        texture         =tex;
        content_ready   =false;     ///<容器就绪≠内容就绪：等生产方画过一次
        async_requested =false;
        async_request_id=0;
        payload_state   =PayloadState::Resident;

        return(true);
    }

    void TextureAsset::MarkContentReady()
    {
        if(!texture||content_ready)
            return;

        content_ready=true;

        GLogDebug(OS_TEXT("[TextureAsset] RT 内容就绪（rt:%s:%s）"),
                  rt_target.c_str(),
                  rt_attachment.c_str());
    }

    void TextureAsset::UnbindRenderTargetTexture(const char *reason)
    {
        if(source_kind!=TextureURIKind::RenderTarget)
            return;

        if(texture)
            GLogInfo(OS_TEXT("[TextureAsset] RT 附件解绑（rt:%s:%s，%s）"),
                     rt_target.c_str(),
                     rt_attachment.c_str(),
                     reason?reason:"未指明原因");

        // 句柄本身必须活着：它可能已经被材质收下，重建后会自动重新绑定。
        texture         =nullptr;
        content_ready   =false;
        async_requested =false;
        async_request_id=0;
        payload_state   =PayloadState::MetaOnly;
    }

    U8String TextureAsset::GetResourceId()const
    {
        switch(source_kind)
        {
            case TextureURIKind::File:
            {
                if(meta.source_path.IsEmpty())
                    return U8String();

                U8String id(u8"texfile:");

                id+=ToU8String(meta.source_path);
                return id;
            }
            case TextureURIKind::RenderTarget:
            {
                if(rt_target.IsEmpty())
                    return U8String();

                U8String id(u8"rtex:");

                id+=ToU8String(rt_target);
                id+=U8String(u8":");
                id+=ToU8String(rt_attachment);
                return id;
            }
            default:
                break;
        }

        return U8String();
    }
}//namespace hgl::graph
