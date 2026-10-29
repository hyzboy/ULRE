#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/asset/GeometryAssetFile.h>
#include<hgl/framework/LoadedFile.h>
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
