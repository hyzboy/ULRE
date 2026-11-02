#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/asset/GeometryAssetFile.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/asset/AssetURI.h>
#include<hgl/asset/LoadedFile.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    namespace
    {
        bool IsTOMLPath(const OSString &filename)
        {
            return filename.EndsWithIgnoreCase(OS_TEXT(".toml"));
        }
    }//namespace

    GeometryAssetRegistry::GeometryAssetRegistry(VulkanDevice *dev,BufferManager *bm)
        :device(dev),buffer_manager(bm)
    {
    }

    GeometryAssetRegistry::~GeometryAssetRegistry()
    {
        Clear();
    }

    GeometryAsset *GeometryAssetRegistry::FindByResourceId(const U8String &resource_id)const
    {
        if(resource_id.IsEmpty())
            return(nullptr);

        for(GeometryAsset *asset:assets)
        {
            if(!asset)
                continue;

            if(asset->GetResourceId()==resource_id)
                return asset;
        }

        return(nullptr);
    }

    GeometryAsset *GeometryAssetRegistry::CreateUniqueFromSpec(const GeometryAssetSpec &spec,
                                                               const GeometryVertexFormat *format_override)
    {
        if(!device)
        {
            GLogError(u8"[GeometryAssetRegistry] 未绑定设备，无法创建几何资产");
            return(nullptr);
        }

        GeometryAsset *asset=new GeometryAsset(this,device,buffer_manager);

        if(!asset)
            return(nullptr);

        if(!asset->SetupFromSpec(spec,format_override))
        {
            delete asset;
            return(nullptr);
        }

        assets.Add(asset);

        return asset;
    }

    GeometryAsset *GeometryAssetRegistry::GetOrCreateFromSpec(const GeometryAssetSpec &spec,
                                                              const GeometryVertexFormat *format_override)
    {
        // builtin 的身份由名字/形状决定，可在创建前算出；file 的身份是路径。
        // 用一次性句柄算身份太重：直接按 path 或 name/shape 内联比较。
        if(spec.kind==GeometryAssetKind::File && !spec.file_path.IsEmpty())
        {
            U8String uri(u8"geofile:");

            uri+=ToU8String(spec.file_path);

            if(GeometryAsset *found=FindByResourceId(uri))
                return found;
        }
        else if(!spec.name.IsEmpty())
        {
            U8String uri(u8"geo:@");

            uri+=ToU8String(AnsiString(spec.name));

            if(GeometryAsset *found=FindByResourceId(uri))
                return found;
        }

        return CreateUniqueFromSpec(spec,format_override);
    }

    GeometryAsset *GeometryAssetRegistry::GetOrLoad(const OSString &filename,
                                                    const GeometryVertexFormat *format_override)
    {
        if(filename.IsEmpty())
        {
            GLogError(u8"[GeometryAssetRegistry] GetOrLoad 收到空文件名");
            return(nullptr);
        }

        if(!device)
        {
            GLogError(u8"[GeometryAssetRegistry] 未绑定设备，无法加载几何资产");
            return(nullptr);
        }

        if(IsTOMLPath(filename))
        {
            GeometryAssetSpec spec;

            if(!LoadGeometryAssetSpecFromTOML(filename,spec))
                return(nullptr);

            // 规约里的 file 路径是**相对资产文件**的，交给调用方保证工作目录；
            // 这与 `.recipe.toml` 里纹理路径的约定一致。
            return GetOrCreateFromSpec(spec,format_override);
        }

        // 裸 `.geo`：按路径去重后建一个 MetaOnly 句柄，等流送系统读盘。
        U8String uri(u8"geofile:");

        uri+=ToU8String(filename);

        if(GeometryAsset *found=FindByResourceId(uri))
            return found;

        GeometryAsset *asset=new GeometryAsset(this,device,buffer_manager);

        if(!asset)
            return(nullptr);

        const GeometryVertexFormat format = format_override
            ? *format_override
            : GetStandardGeometryVertexFormat();

        if(!asset->SetupFromMiniPackFile(filename,format))
        {
            delete asset;
            return(nullptr);
        }

        assets.Add(asset);

        return asset;
    }

    GeometryAsset *GeometryAssetRegistry::GetOrLoadURI(const OSString &uri,
                                                      const GeometryVertexFormat *format_override)
    {
        if(uri.IsEmpty())
        {
            GLogError(u8"[GeometryAssetRegistry] GetOrLoadURI 收到空 URI");
            return(nullptr);
        }

        const asset::AssetURI parsed = asset::AssetURI::Parse(uri,asset::AssetType::Geometry);

        // 类型段写的是别的资产（`tex:...`）⇒ 走错门了，明确报错而不是当路径去读盘。
        if(parsed.HasType() && parsed.type != asset::AssetType::Geometry)
        {
            GLogError(u8"[GeometryAssetRegistry] URI 类型不是几何：%s（实际 gtype=\"%s\"）",
                      asset::AssetURISegmentToAnsi(uri).c_str(),
                      asset::GetAssetTypeName(parsed.type));
            return(nullptr);
        }

        if(parsed.IsBuiltin())
        {
            const GeometryAssetSpec *spec = GeometryPresetLibrary::Instance().Get(
                asset::AssetURISegmentToAnsi(parsed.target));

            if(!spec)
            {
                GLogError(u8"[GeometryAssetRegistry] 内置几何预设不存在：%s",
                          asset::AssetURISegmentToAnsi(parsed.target).c_str());
                return(nullptr);
            }

            // 内置几何是内存规约 ⇒ 建出来即 Resident，没有流送窗口。
            return GetOrCreateFromSpec(*spec,format_override);
        }

        if(parsed.source == asset::AssetSourceKind::RenderTarget)
        {
            GLogError(u8"[GeometryAssetRegistry] 几何不支持 rt: 来源：%s",
                      asset::AssetURISegmentToAnsi(uri).c_str());
            return(nullptr);
        }

        // 其余全部按路径处理：`file:` 显式来源与裸路径在这里合流。
        return GetOrLoad(parsed.target,format_override);
    }

    int GeometryAssetRegistry::GetPendingCount()const
    {
        int count=0;

        for(GeometryAsset *asset:assets)
        {
            if(asset && !asset->IsReady() && !asset->IsAsyncRequested())
                ++count;
        }

        return(count);
    }

    int GeometryAssetRegistry::GetResidentCount()const
    {
        int count=0;

        for(GeometryAsset *asset:assets)
        {
            if(asset && asset->IsReady())
                ++count;
        }

        return(count);
    }

    void GeometryAssetRegistry::Clear()
    {
        // 句柄析构会把各自的 Geometry 带走（见 GeometryAsset::~GeometryAsset）。
        for(GeometryAsset *asset:assets)
            delete asset;

        assets.Clear();
    }
}//namespace hgl::graph
