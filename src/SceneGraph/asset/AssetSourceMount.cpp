#include<hgl/graph/asset/AssetSourceMount.h>

#include<hgl/asset/AssetSource.h>
#include<hgl/asset/AssetStream.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/Log.h>

DEFINE_LOGGER_MODULE(AssetMount)
USE_MODULE_LOGGER(AssetMount)

namespace hgl::graph
{
    using namespace hgl;

    bool MountAssetSource(const AssetSourceMountConfig &config,AnsiString *out_error)
    {
        if(out_error)
            out_error->Clear();

        if(config.name.IsEmpty()||config.root.IsEmpty())
        {
            if(out_error)
                *out_error="mount: empty name or root";
            return false;
        }

        // 幂等：同名已经挂上就直接成功。GraphicsContext 会在每个设备初始化时
        // 调一次本函数，而挂载表是进程级的 —— 重复挂载必须是无害的。
        if(asset::GetSource(config.name))
        {
            FLogInfo(u8"包裹 <%s> 已挂载，跳过",config.name.c_str());
            return true;
        }

        if(!filesystem::IsDirectory(config.root))
        {
            if(out_error)
                *out_error="mount: root is not a directory";
            FLogWarning(OS_TEXT("包裹 <")+ToOSString(config.name.c_str())+OS_TEXT("> 的根目录不存在：")+config.root);
            return false;
        }

        asset::AssetsSourceMountOptions options;

        options.build_index   = config.build_index;
        options.load_manifest = config.load_manifest;
        options.save_manifest = config.save_manifest;

        asset::AssetSource *src=asset::CreateSourceByFilesystem(config.name,config.root,
                                                               config.only_read,options);

        if(!src)
        {
            if(out_error)
                *out_error="mount: CreateSourceByFilesystem failed";
            FLogError(OS_TEXT("包裹 <")+ToOSString(config.name.c_str())+OS_TEXT("> 挂载失败：")+config.root);
            return false;
        }

        const asset::AssetIndexTable *index=src->GetIndexTable();
        const int indexed_count=(index?index->GetCount():0);

        // 注意：root 是 `OSString`（Windows 下为 UTF-16），不能用 u8 格式串的 `%s` 打印，
        // 否则只会输出首字符。这里统一走 OSString 拼接。
        FLogInfo(OS_TEXT("包裹 <")+ToOSString(config.name.c_str())+OS_TEXT("> 已挂载：root=")
                 +config.root+OS_TEXT(" 索引项=")+OSString::numberOf(indexed_count));

        if(index==nullptr)
            FLogWarning(u8"包裹 <%s> 未建索引，只能按物理路径取流",config.name.c_str());

        return true;
    }

    bool MountDefaultAssetSources(AnsiString *out_error)
    {
        AssetSourceMountConfig config;

        config.name = DEFAULT_ASSET_SOURCE_NAME;
        config.root = DEFAULT_ASSET_SOURCE_ROOT;

        // 早期示例仍走裸物理路径，缺 `res/` 不算致命错误。
        if(!filesystem::IsDirectory(config.root))
        {
            if(out_error)
                out_error->Clear();
            FLogNotice(u8"未发现 res/ 目录，跳过默认包裹挂载");
            return false;
        }

        return MountAssetSource(config,out_error);
    }

    void UnmountDefaultAssetSources()
    {
        if(asset::UnregisterAssetsSource(DEFAULT_ASSET_SOURCE_NAME))
            FLogInfo(u8"包裹 <%s> 已注销",DEFAULT_ASSET_SOURCE_NAME);
    }

    io::InputStream *OpenAssetInputStream(const asset::AssetURI &uri,AnsiString *out_error)
    {
        return asset::OpenAssetStream(uri,out_error);
    }

    io::InputStream *OpenAssetInputStream(const OSString &uri_text,
                                          asset::AssetType default_type,
                                          AnsiString *out_error)
    {
        const asset::AssetURI uri=asset::AssetURI::Parse(uri_text,default_type);

        // 盘符路径（`C:\a\b.scene`）会被 URI 解析器把 `C` 当成类型段、把 `\a\b.scene`
        // 当成定位段。来源仍是 Unknown 且定位段已不是原文 ⇒ 这不是 URI，按裸路径开。
        if(uri.source==asset::AssetSourceKind::Unknown&&!(uri.target==uri_text))
        {
            io::FileInputStream *fis=new io::FileInputStream;

            if(fis->Open(uri_text))
                return fis;

            delete fis;

            if(out_error)
                *out_error="cannot open file";
            return nullptr;
        }

        return asset::OpenAssetStream(uri,out_error);
    }
}
