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
        async_requested = false;
        async_request_id= 0;
    }

    bool TextureAsset::OnLoadPayload()
    {
        if(texture)
            return(true);

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

        meta.source_path    =filename;
        meta.asset_type     =OS_TEXT("Texture2D");
        this->auto_mipmaps  =auto_mipmaps;

        return(Init());
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
        async_requested =false;
        async_request_id=0;
        payload_state   =PayloadState::LoadFailed;
    }
}//namespace hgl::graph
