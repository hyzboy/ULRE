#include<hgl/graph/asset/PrimitiveAssetRegistry.h>
#include<hgl/graph/asset/PrimitiveAssetFile.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/mtl/MaterialRecipeRegistry.h>
#include<hgl/filesystem/Path.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    namespace
    {
        /// 相对 `geometry` 路径按 TOML 所在目录解析；绝对路径原样返回。
        OSString ResolveRelative(const OSString &base_dir,const OSString &path)
        {
            if(base_dir.IsEmpty()||path.IsEmpty())
                return path;

            // Windows 盘符绝对路径 / 以分隔符开头都视为绝对路径。
            const os_char first=path.c_str()[0];

            if(first==OS_TEXT('/')||first==OS_TEXT('\\'))
                return path;

            if(path.Length()>1&&path.c_str()[1]==OS_TEXT(':'))
                return path;

            return (hgl::filesystem::Path(base_dir)/path).ToOSString();
        }
    }//namespace

    PrimitiveAssetRegistry::~PrimitiveAssetRegistry()
    {
        Clear();
    }

    PrimitiveAssetRegistry::Entry *PrimitiveAssetRegistry::FindEntry(const U8String &resource_id)const
    {
        if(resource_id.IsEmpty())
            return(nullptr);

        for(Entry *entry:entries)
        {
            if(entry&&entry->resource_id==resource_id)
                return(entry);
        }

        return(nullptr);
    }

    PrimitiveAssetRegistry::Entry *PrimitiveAssetRegistry::BuildEntry(const PrimitiveAssetSpec &spec,
                                                                      const U8String &resource_id)
    {
        if(!geometry_registry||!recipe_registry)
        {
            GLogError(u8"[PrimitiveAssetRegistry] 未绑定几何/配方注册器，无法装配图元资产");
            return(nullptr);
        }

        GeometryAsset *geometry_asset=geometry_registry->GetOrLoad(spec.geometry_path);

        if(!geometry_asset)
        {
            GLogError(u8"[PrimitiveAssetRegistry] 几何资产解析失败：%s",
                      reinterpret_cast<const char *>(ToU8String(spec.geometry_path).c_str()));
            return(nullptr);
        }

        const mtl::MaterialRecipe *surface_recipe=
            recipe_registry->FindByName(spec.recipe_name.c_str());

        if(!surface_recipe)
        {
            GLogError(u8"[PrimitiveAssetRegistry] 材质配方未找到：%s（图元 %s）",
                      spec.recipe_name.c_str(),
                      reinterpret_cast<const char *>(resource_id.c_str()));
            return(nullptr);
        }

        Entry *entry=new Entry;

        if(!entry)
            return(nullptr);

        entry->resource_id=resource_id;

        // 主变体(Surface)恒为第 0 条：`GetDefaultVariant()` / 运行期默认索引都依赖这一约定。
        entry->variants.Reserve(spec.extra_variants.GetCount()+1);
        PrimitiveVariant surface_variant{};

        surface_variant.material_recipe=surface_recipe;
        surface_variant.purpose       =PrimitiveVariantPurpose::Surface;
        surface_variant.lod_index     =0;

        entry->variants.Add(surface_variant);

        for(PrimitiveVariantSpec *variant_spec:spec.extra_variants)
        {
            const mtl::MaterialRecipe *recipe=
                recipe_registry->FindByName(variant_spec->recipe_name.c_str());

            if(!recipe)
            {
                GLogError(u8"[PrimitiveAssetRegistry] 变体材质配方未找到：%s（图元 %s）",
                          variant_spec->recipe_name.c_str(),
                          reinterpret_cast<const char *>(resource_id.c_str()));

                delete entry;
                return(nullptr);
            }

            PrimitiveVariant variant{};

            variant.material_recipe=recipe;
            variant.purpose       =variant_spec->purpose;
            variant.lod_index     =variant_spec->lod_index;

            entry->variants.Add(variant);
        }

        // 变体数组已定长：此后 asset 里存的是这份存储的裸指针，绝不能再 Add/Wait 扩容。
        entry->asset=PrimitiveAsset(geometry_asset->GetGeometry(),
                                    geometry_asset,
                                    entry->variants.GetData(),
                                    static_cast<uint32_t>(entry->variants.GetCount()),
                                    spec.primitive_type);

        if(!entry->asset.IsDeclared())
        {
            GLogError(u8"[PrimitiveAssetRegistry] 装配出的图元资产非法：%s",
                      reinterpret_cast<const char *>(resource_id.c_str()));
            delete entry;
            return(nullptr);
        }

        entries.Add(entry);

        return(entry);
    }

    PrimitiveAsset *PrimitiveAssetRegistry::GetOrLoad(const OSString &filename)
    {
        if(filename.IsEmpty())
        {
            GLogError(u8"[PrimitiveAssetRegistry] GetOrLoad 收到空文件名");
            return(nullptr);
        }

        U8String resource_id(u8"prmfile:");

        resource_id+=ToU8String(filename);

        if(Entry *found=FindEntry(resource_id))
            return(const_cast<PrimitiveAsset *>(&found->asset));

        PrimitiveAssetSpec spec;

        if(!LoadPrimitiveAssetSpecFromTOML(filename,spec))
            return(nullptr);

        const OSString base_dir=
            (hgl::filesystem::Path(filename).GetParent()).ToOSString();

        spec.geometry_path=ResolveRelative(base_dir,spec.geometry_path);

        Entry *entry=BuildEntry(spec,resource_id);

        return(entry?&entry->asset:nullptr);
    }

    PrimitiveAsset *PrimitiveAssetRegistry::GetOrCreate(const PrimitiveAssetSpec &spec)
    {
        if(!spec.IsValid())
        {
            GLogError(u8"[PrimitiveAssetRegistry] GetOrCreate 收到非法规约");
            return(nullptr);
        }

        U8String resource_id;

        if(!spec.name.IsEmpty())
        {
            resource_id=U8String(u8"prm:@");
            resource_id+=ToU8String(AnsiString(spec.name));
        }
        else
        {
            resource_id=U8String(u8"prmfile:");
            resource_id+=ToU8String(spec.geometry_path);
        }

        if(Entry *found=FindEntry(resource_id))
            return(const_cast<PrimitiveAsset *>(&found->asset));

        Entry *entry=BuildEntry(spec,resource_id);

        return(entry?&entry->asset:nullptr);
    }

    const PrimitiveAsset *PrimitiveAssetRegistry::GetAt(const int index)const
    {
        if(index<0||index>=entries.GetCount())
            return(nullptr);

        const Entry *entry=entries[index];

        return(entry?&entry->asset:nullptr);
    }

    const PrimitiveAsset *PrimitiveAssetRegistry::FindByUri(const U8String &resource_id)const
    {
        const Entry *entry=FindEntry(resource_id);

        return(entry?&entry->asset:nullptr);
    }

    void PrimitiveAssetRegistry::Clear()
    {
        for(Entry *entry:entries)
            delete entry;

        entries.Clear();
    }
}//namespace hgl::graph
