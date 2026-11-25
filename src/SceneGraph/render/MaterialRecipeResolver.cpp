#include <hgl/graph/core/MaterialRecipeResolver.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/asset/AssetIndex.h>
#include <hgl/asset/AssetSource.h>
#include <hgl/io/InputStream.h>
#include <hgl/mtl/MaterialRecipeInherit.h>
#include <hgl/type/StdString.h>
#include <hgl/log/Log.h>
#include <cstring>

namespace hgl::graph
{
    namespace
    {
        /**
         * 「包内文件名检索」：在所有已挂载包裹的内存索引表里为请求串找到
         * 材质配方（`MaterialRecipe`）唯一的逻辑路径。
         *
         * 匹配优先级：逻辑路径全等 > 路径后缀匹配（请求是逻辑路径的尾段）>
         * 末段（短名）匹配。命中后拼出 `rcp:pack(<挂载名>):<逻辑路径>` ——
         * 物理扩展名（`.recipe.toml` / `.recipe.fb`）从不出现在引用里，
         * 由索引裁决当前该用哪个编码格式。
         */
        bool FindRecipeLogicalPathInPacks(const char *request, OSString &out_uri)
        {
            out_uri.Clear();

            ManagedArray<AnsiString> pack_names;
            if (ListMountedAssetSourceNames(pack_names) <= 0)
                return false;

            const AnsiString ansi_request(request);
            const int req_len = ansi_request.Length();
            if (req_len <= 0)
                return false;

            const U8String u8_request(reinterpret_cast<const u8char *>(ansi_request.c_str()),
                                      req_len);

            // 「文件名 = 配方名去掉最后一段」的例外兜底：语料里只有两处
            // `PBRSpheres.recipe.toml → name="PBRSpheres.Lit"`、
            // `TextureQuad.recipe.toml → name="TextureQuad.UnlitTexture"`
            // （见 doc/material-shader-naming-and-layout.md §5.3）。请求的完整末段
            // 匹配不上时，再用「去掉最后一个 `.` 段」的主干重试一次末段匹配。
            const AnsiString ansi_stripped = ansi_request.Left(ansi_request.FindRightChar('.'));
            U8String u8_stripped;
            const int stripped_len = ansi_stripped.Length();
            if (stripped_len > 0)
                u8_stripped = U8String(
                    reinterpret_cast<const u8char *>(ansi_stripped.c_str()), stripped_len);

            int best_kind = 0;   // 4=全等 3=后缀 2=末段 1=去尾段后的末段
            AnsiString best_pack;
            U8String best_logical;

            ManagedArray<asset::AssetIndexEntry> entries;
            for (int p = 0; p < pack_names.GetCount(); ++p)
            {
                asset::AssetSource *src = asset::GetSource(*pack_names[p]);
                if (!src)
                    continue;

                entries.Clear();
                if (src->GetEntriesByType(asset::AssetType::MaterialRecipe, entries) <= 0)
                    continue;

                for (int i = 0; i < entries.GetCount(); ++i)
                {
                    const U8String &logical = entries[i]->logical_path;
                    const int len = logical.Length();
                    if (len <= 0)
                        continue;

                    int kind = 0;
                    if (len == req_len && logical.Comp(0, u8_request) == 0)
                        kind = 4;
                    else if (len > req_len &&
                             logical.Comp(len - req_len, u8_request) == 0 &&
                             logical.c_str()[len - req_len - 1] == u8char('/'))
                        kind = 3;
                    else
                    {
                        const int slash = logical.FindRightChar(u8char('/'));
                        const int base = (slash < 0) ? 0 : slash + 1;
                        if (len - base == req_len && logical.Comp(base, u8_request) == 0)
                            kind = 2;
                        else if (stripped_len > 0 &&
                                 len - base == stripped_len &&
                                 logical.Comp(base, u8_stripped) == 0)
                            kind = 1;
                    }

                    if (kind > best_kind)
                    {
                        best_kind = kind;
                        best_pack = *pack_names[p];
                        best_logical = logical;
                    }
                }
            }

            if (best_kind == 0)
                return false;

            out_uri = OSString(OS_TEXT("rcp:pack(")) + ToOSString(best_pack.c_str())
                    + OS_TEXT("):")
                    + ToOSString(reinterpret_cast<const char *>(best_logical.c_str()));
            return true;
        }
    }//anonymous namespace

    bool MaterialRecipeResolver::LoadFromURI(const OSString &uri_text,
                                             AnsiString &out_recipe_name)
    {
        out_recipe_name.Clear();

        AnsiString err;
        io::InputStream *stream = OpenAssetInputStream(uri_text,
                                                       asset::AssetType::MaterialRecipe,
                                                       &err);
        if (!stream)
        {
            GLogError(u8"[MaterialRecipeResolver] 配方取流失败: uri=%ls reason=%s",
                      uri_text.c_str(), err.c_str());
            return false;
        }

        // `LoadFromStream` 不消费流的所有权（契约见其声明），这里负责 delete。
        const bool ok = registry->LoadFromStream(
            stream, uri_text.c_str(), CurrentTextureManager(), &out_recipe_name);
        delete stream;

        if (!ok && out_recipe_name.IsEmpty())
        {
            GLogError(u8"[MaterialRecipeResolver] 配方解析/注册失败: uri=%ls", uri_text.c_str());
            return false;
        }

        if (!ok)
        {
            // 重名撞上已存在的句柄：名字照样可用，调用方按名继续查即可。
            GLogWarning(u8"[MaterialRecipeResolver] 配方已注册（重名命中已有句柄）: uri=%ls recipe=%s",
                        uri_text.c_str(), out_recipe_name.c_str());
        }

        return true;
    }

    bool MaterialRecipeResolver::LocateURI(const char *request, OSString &out_uri)
    {
        out_uri.Clear();

        // 带 scheme（`rcp:` / `file:`）或盘符 ⇒ 整个串交给 URI 分派器解读。
        if (strchr(request, ':') != nullptr)
        {
            out_uri = ToOSString(request);
            return true;
        }

        // 否则走包裹的文件名检索系统（短名与包内相对路径都在这一次内存扫描里解决）。
        if (FindRecipeLogicalPathInPacks(request, out_uri))
            return true;

        // 检索不到但形如相对路径 ⇒ 保留「直接给物理路径」的老写法：按本地文件开流
        // （`OpenAssetInputStream` 对裸路径分派）。短名不走这条，避免误 hit 错文件。
        if (strchr(request, '/') != nullptr || strchr(request, '\\') != nullptr)
        {
            out_uri = ToOSString(request);
            return true;
        }

        return false;
    }

    const mtl::MaterialRecipe *MaterialRecipeResolver::Resolve(const char *request,
                                                               const int depth)
    {
        if (!request || !request[0])
        {
            GLogError(u8"[MaterialRecipeResolver] 无法查找材质配方：名称为空");
            return nullptr;
        }

        if (!registry)
            return nullptr;

        if (const mtl::MaterialRecipe *recipe = registry->FindByName(request))
            return recipe;

        const AnsiString key(request);

        AnsiString registered;
        if (const AnsiString *cached = lookup_cache.GetValuePointer(key))
        {
            if (cached->IsEmpty())
                return nullptr;   // 已确认取不到，不再重试取流

            registered = *cached;
            if (const mtl::MaterialRecipe *recipe = registry->FindByName(registered.c_str()))
                return recipe;
        }
        else
        {
            OSString uri_text;
            if (!LocateURI(request, uri_text))
            {
                GLogError(u8"[MaterialRecipeResolver] 材质配方未找到: recipe=%s registered=%d",
                          request, registry->GetCount());
                lookup_cache.Add(key, AnsiString());
                return nullptr;
            }

            AnsiString name;
            if (!LoadFromURI(uri_text, name))
            {
                lookup_cache.Add(key, AnsiString());
                return nullptr;
            }

            registered = name;
            lookup_cache.Add(key, registered);

            if (const mtl::MaterialRecipe *recipe = registry->FindByName(registered.c_str()))
            {
                GLogDebug(u8"[MaterialRecipeResolver] 材质配方已按需加载: request=%s uri=%ls domain=%s",
                          request, uri_text.c_str(),
                          mtl::GetMaterialDomainName(recipe->domain));
                return recipe;
            }
        }

        // 到这里：句柄在表里但没到 `Ready` ⇒ 它的 `parent` 还没就绪。按需补加载
        // 父配方；父注册时触发的 `ResolvePending` 会把本条推进到 `Ready`。
        if (depth >= kMaxParentDepth)
        {
            GLogError(u8"[MaterialRecipeResolver] 配方父链过深（疑似成环）: recipe=%s",
                      registered.c_str());
            lookup_cache.GetValueRef(key) = AnsiString();
            return nullptr;
        }

        const mtl::MaterialRecipeAsset *asset = registry->FindAssetByName(registered.c_str());
        if (!asset)
        {
            // 句柄在等待期间被摘除（校验失败 / 父链成环收尾）⇒ 本请求不可满足。
            GLogError(u8"[MaterialRecipeResolver] 配方句柄已摘除: recipe=%s", registered.c_str());
            lookup_cache.GetValueRef(key) = AnsiString();
            return nullptr;
        }

        const AnsiString &parent_text = asset->GetSpec().parent;
        mtl::MaterialRecipeParentRef parent_ref;
        if (parent_text.IsEmpty() ||
            !mtl::ParseMaterialRecipeParentRef(parent_text.c_str(), parent_ref) ||
            parent_ref.IsNone())
        {
            GLogError(u8"[MaterialRecipeResolver] 配方停在等待态但父引用不可解析: recipe=%s parent=%s",
                      registered.c_str(), parent_text.c_str());
            lookup_cache.GetValueRef(key) = AnsiString();
            return nullptr;
        }

        bool parent_loaded = false;
        if (parent_ref.kind == mtl::MaterialRecipeParentRefKind::ByName)
        {
            parent_loaded = Resolve(parent_ref.name.c_str(), depth + 1) != nullptr;
        }
        else
        {
            AnsiString parent_name;
            if (LoadFromURI(parent_ref.path, parent_name) && !parent_name.IsEmpty())
            {
                const mtl::MaterialRecipeAsset *parent_asset =
                    registry->FindAssetByName(parent_name.c_str());
                parent_loaded = parent_asset && parent_asset->IsReady();
            }
        }

        if (const mtl::MaterialRecipe *recipe = registry->FindByName(registered.c_str()))
            return recipe;

        GLogError(u8"[MaterialRecipeResolver] 材质配方父链无法解析: recipe=%s parent=%s parent_loaded=%d",
                  registered.c_str(), parent_text.c_str(), parent_loaded ? 1 : 0);
        lookup_cache.GetValueRef(key) = AnsiString();
        return nullptr;
    }
}