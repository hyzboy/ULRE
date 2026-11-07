#include<hgl/graph/asset/AssetSourceMount.h>

#include<hgl/asset/AssetSource.h>
#include<hgl/asset/AssetStream.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/Log.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/type/UnorderedMap.h>

DEFINE_LOGGER_MODULE(AssetMount)
USE_MODULE_LOGGER(AssetMount)

namespace hgl::graph
{
    using namespace hgl;

    namespace
    {
        /// 引擎侧保留的挂载根表。
        ///
        /// `AssetSource` 只提供「包裹内路径 → 包裹内路径」的解析，不暴露挂载根；
        /// 而异步读盘（`LoadedFileManager::Submit`）只吃**物理路径**。这里在挂载
        /// 成功时记下根，就能把 `pack(res):x/y` 换成真实磁盘路径，避免为了拿到
        /// 一个路径把整包同步读进内存。
        UnorderedMap<AnsiString, OSString> mount_roots;

        const OSString *FindMountRoot(const AnsiString &name)
        {
            return mount_roots.GetValuePointer(name);
        }

        void SetMountRoot(const AnsiString &name,const OSString &root)
        {
            if(!mount_roots.Add(name,root))
                mount_roots.GetValueRef(name)=root;
        }

        void RemoveMountRoot(const AnsiString &name)
        {
            mount_roots.DeleteByKey(name);
        }
    }//anonymous namespace

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

        SetMountRoot(config.name,config.root);

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

    void UnmountAssetSource(const AnsiString &name)
    {
        if(asset::UnregisterAssetsSource(name))
            FLogInfo(u8"包裹 <%s> 已注销",name.c_str());

        RemoveMountRoot(name);
    }

    void UnmountDefaultAssetSources()
    {
        UnmountAssetSource(DEFAULT_ASSET_SOURCE_NAME);
    }

    bool ResolveAssetPhysicalPath(const OSString &uri_text,asset::AssetType default_type,OSString &out_path)
    {
        out_path.Clear();

        const asset::AssetURI uri=asset::AssetURI::Parse(uri_text,default_type);

        if(uri.IsPack())
        {
            const OSString *root=FindMountRoot(uri.pack_name);

            if(root==nullptr)
                return false;

            // 先试索引反查：`id:<32 hex>` 与「不带格式后缀的逻辑路径」在磁盘上
            // 根本没有对应文件名，直接拼路径必然失败。索引里记着真正的包内相对
            // 路径（含扩展名），并且已经裁决过 `.toml` / `.fb` 同名撞车该用哪个 ——
            // 这条通道同时也是「异步读盘不要为了拿一个路径把整包读进内存」的关键。
            asset::AssetSource *src=asset::GetSource(uri.pack_name);

            U8String pack_path;

            if(src!=nullptr&&src->HasIndexTable())
            {
                if(uri.IsIDTarget())
                {
                    // 身份寻址没有「按路径摸文件」的退路：查不到就是不存在。
                    asset::AssetID id;

                    if(uri.GetIDTarget(id)&&src->ResolvePath(id,pack_path)&&!pack_path.IsEmpty())
                    {
                        out_path=filesystem::JoinPathWithFilename(*root,ToOSString(pack_path));

                        if(filesystem::FileExist(out_path))
                            return true;

                        out_path.Clear();
                    }

                    return false;
                }

                if(uri.HasType()&&src->ResolvePath(uri.type,ToU8String(uri.target),pack_path)&&!pack_path.IsEmpty())
                {
                    out_path=filesystem::JoinPathWithFilename(*root,ToOSString(pack_path));

                    if(filesystem::FileExist(out_path))
                        return true;

                    out_path.Clear();
                }
            }

            // 原始素材（类型段就是物理后缀，索引里没有它的条目）才走直通拼接。
            out_path=filesystem::JoinPathWithFilename(*root,uri.target);

            if(filesystem::FileExist(out_path))
                return true;

            out_path.Clear();
            return false;
        }

        // `builtin` / `rt` / 无名 `pack` 没有磁盘对应物。
        if(uri.HasSource())
            return false;

        // 裸路径或 `file:`：定位段就是物理路径。盘符形态（`C:\a\b`）会被解析器
        // 把盘符当类型段，此时 `target` 已不是原文，直接按原文当路径试。
        out_path=(uri.target==uri_text)?uri.target:uri_text;

        if(filesystem::FileExist(out_path))
            return true;

        out_path.Clear();
        return false;
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

    asset::AssetSource *GetMountedAssetSource(const char *name)
    {
        if(name==nullptr||*name==0)
            name=DEFAULT_ASSET_SOURCE_NAME;

        return asset::GetSource(AnsiString(name));
    }

    int ListAssetDirectory(const AnsiString &dir_path,ManagedArray<asset::SourceEntry> &out,
                           const char *source_name)
    {
        asset::AssetSource *src=GetMountedAssetSource(source_name);

        if(src==nullptr)
            return -1;

        // `u8char` 在开 `HGL_SUPPORT_CHAR8_T` 时是 `char8_t`，与 `char` 不是同一类型，
        // 没有隐式转换。字节内容本来相同，直接重解释。
        const U8String u8_dir(reinterpret_cast<const u8char *>(dir_path.c_str()),dir_path.Length());

        return src->ListDirectory(u8_dir,out);
    }
}
