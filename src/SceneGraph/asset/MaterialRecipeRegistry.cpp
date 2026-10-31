#include <hgl/mtl/MaterialRecipeRegistry.h>
#include <hgl/mtl/MaterialRecipeInherit.h>
#include <hgl/mtl/MaterialResolver.h>
#include <hgl/mtl/DomainEntrySource.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/graph/asset/TextureAsset.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

#include <climits>

namespace hgl::graph::mtl
{
    namespace
    {
        bool IsRecipeFile(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".recipe.toml"));
        }

        void LogRecipeParseFailure(
            const os_char *label,
            const MaterialRecipeFileParseResult result,
            const MaterialRecipeFileParseError &error)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方解析失败: %ls (%s)"
                      u8" line=%d table=%s key=%s value=\"%s\" detail=%s",
                      label,
                      GetMaterialRecipeFileParseResultName(result),
                      error.line,
                      error.table.c_str(),
                      error.key.c_str(),
                      error.value.c_str(),
                      error.detail.c_str());
        }
    }

    bool MaterialRecipeRegistry::ResolveAndValidate(
        MaterialRecipeAsset &asset,
        hgl::graph::TextureManager *texture_manager)
    {
        MaterialRecipe &recipe = asset.EditRecipe();

        // 配方自带 `domain` ⇒ 纹理槽按**领域契约**校验（契约是唯一真源）。
        const MaterialDomainContract *contract =
            GetMaterialDomainContract(recipe.domain);

        if (!contract)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方未声明有效领域: recipe=%s domain=%s",
                      recipe.recipe_name.c_str(),
                      GetMaterialDomainName(recipe.domain));
            return false;
        }

        // 契约存在 ≠ 能编出着色器：入口源表里没有的领域只有「应该长什么样」，
        // 没有主着色器。加载期就拒绝，避免运行期才在构建里失败。
        if (!HasMaterialDomainEntrySource(recipe.domain))
        {
            GLogError(u8"[MaterialRecipeRegistry] 该领域尚无固定主着色器入口: recipe=%s domain=%s",
                      recipe.recipe_name.c_str(),
                      contract->name);
            return false;
        }

        // 表面模块只有 `no_material_source` 领域可用，且必须是入口源认得的形态。
        if (!recipe.surface_module_name.empty())
        {
            DomainEntrySource probe{};
            if (!contract->no_material_source
             || !ResolveDomainEntrySource(recipe.domain,
                                          ShaderProgramPurpose::ForwardColor,
                                          false,
                                          probe,
                                          recipe.surface_module_name.c_str()))
            {
                GLogError(u8"[MaterialRecipeRegistry] 配方表面模块不可用: recipe=%s surface=%s domain=%s",
                          recipe.recipe_name.c_str(),
                          recipe.surface_module_name.c_str(),
                          contract->name);
                return false;
            }
        }

        for (RecipeTextureBinding &binding : recipe.textures)
        {
            MaterialDomainTextureSlot domain_slot{};

            const MaterialDomainTextureSlot *slot =
                FindMaterialDomainTextureSlot(recipe.domain,
                                              binding.texture_name.c_str());
            if (!slot)
            {
                GLogError(u8"[MaterialRecipeRegistry] 配方纹理槽不在领域契约内: recipe=%s slot=%s domain=%s",
                          recipe.recipe_name.c_str(),
                          binding.texture_name.c_str(),
                          contract->name);
                return false;
            }

            domain_slot = *slot;
            const SamplerType sampler_type = slot->sampler;

            if (binding.array_layer != 0
             && !IsMaterialTextureArraySampler(sampler_type))
            {
                GLogError(u8"[MaterialRecipeRegistry] 非数组纹理不能指定非零 layer: recipe=%s slot=%s layer=%u",
                          recipe.recipe_name.c_str(),
                          binding.texture_name.c_str(),
                          binding.array_layer);
                return false;
            }

            // 契约要求必须声明的槽，配方给了就必须真的绑定资源
            // （`required = false` 是"可缺"语义，与契约的 must_declare 不是一回事）。
            if (domain_slot.must_declare && binding.resource_id.empty())
            {
                GLogError(u8"[MaterialRecipeRegistry] 领域契约要求该槽必须给出资源: recipe=%s slot=%s domain=%s",
                          recipe.recipe_name.c_str(),
                          binding.texture_name.c_str(),
                          contract->name);
                return false;
            }
        }

        if (!recipe.textures.empty() && !texture_manager)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方声明纹理但没有 TextureManager: recipe=%s",
                      recipe.recipe_name.c_str());
            return false;
        }

        for (RecipeTextureBinding &binding : recipe.textures)
        {
            hgl::graph::TextureAsset *texture_asset =
                texture_manager->CreateTextureAsset(
                    ToOSString(binding.resource_id.c_str()),
                    binding.auto_mipmaps);
            if (!texture_asset)
            {
                GLogError(u8"[MaterialRecipeRegistry] 创建纹理句柄失败: recipe=%s slot=%s resource=%s",
                          recipe.recipe_name.c_str(),
                          binding.texture_name.c_str(),
                          binding.resource_id.c_str());
                return false;
            }

            if (!recipe.AddTexture(binding.texture_name,
                                   texture_asset,
                                   binding.required,
                                   binding.array_layer))
            {
                GLogError(u8"[MaterialRecipeRegistry] 无法绑定纹理句柄: recipe=%s slot=%s",
                          recipe.recipe_name.c_str(),
                          binding.texture_name.c_str());
                return false;
            }
        }

        NormalizeRecipe(recipe);
        if (!IsRecipeNormalized(recipe))
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方规范化失败: recipe=%s domain=%s",
                      recipe.recipe_name.c_str(),
                      GetMaterialDomainName(recipe.domain));
            return false;
        }
        return true;
    }

    bool MaterialRecipeRegistry::LoadFile(
        const OSString &path,
        hgl::graph::TextureManager *texture_manager)
    {
        hgl::io::OpenFileInputStream opener(path);
        if (!opener)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方文件打不开: %ls", path.c_str());
            return false;
        }

        const int64 size = opener->GetSize();
        if (size <= 0 || size > INT_MAX)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方文件大小无效: %ls (%lld)",
                      path.c_str(), static_cast<long long>(size));
            return false;
        }

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方文件读取失败: %ls", path.c_str());
            return false;
        }
        buffer[static_cast<size_t>(size)] = '\0';

        return RegisterRecipeSource(path.c_str(),
                                    buffer.data(),
                                    static_cast<int>(size),
                                    texture_manager);
    }

    bool MaterialRecipeRegistry::LoadDirectory(
        const OSString &directory,
        hgl::graph::TextureManager *texture_manager,
        int *out_file_count,
        int *out_error_count)
    {
        if (!hgl::filesystem::Path(directory).IsDirectory())
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方目录不存在或不可访问: %ls",
                      directory.c_str());
            return false;
        }

        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;
        const int scan_count = hgl::filesystem::GetFileInfoList(
            file_list, directory, true, true, true);
        if (scan_count < 0)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方目录扫描失败: %ls (result=%d)",
                      directory.c_str(), scan_count);
            return false;
        }

        int file_count = 0;
        int error_count = 0;
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &file = file_list[i];
            if (!file.is_file || !IsRecipeFile(OSString(file.name)))
                continue;

            if (LoadFile(OSString(file.fullname), texture_manager))
                ++file_count;
            else
                ++error_count;
        }

        // 目录是批次边界：扫完整目录后再收尾，父子配方同目录内谁先扫到都无所谓。
        const int unresolved = FinalizePending();
        file_count -= unresolved;
        error_count += unresolved;

        if (out_file_count)
            *out_file_count = file_count;
        if (out_error_count)
            *out_error_count = error_count;
        GLogInfo(u8"[MaterialRecipeRegistry] 配方目录加载完成: directory=%ls loaded=%d errors=%d total=%d",
                 directory.c_str(), file_count, error_count, files.GetCount());
        return true;
    }

    bool MaterialRecipeRegistry::RegisterRecipeSource(
        const os_char *label,
        const char *content,
        const int content_size,
        hgl::graph::TextureManager *texture_manager)
    {
        const os_char *source_label = label ? label : OS_TEXT("<memory>");

        MaterialRecipeFileData data;
        MaterialRecipeSpec spec;
        const MaterialRecipeFileParseResult parse_result =
            ParseMaterialRecipeFile(content, content_size, data, &spec);
        if (parse_result != MaterialRecipeFileParseResult::OK)
        {
            LogRecipeParseFailure(source_label, parse_result, data.parse_error);
            return false;
        }

        if (FindAssetByName(data.recipe.recipe_name.c_str()))
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方名称重复: %s (source=%ls)",
                      data.recipe.recipe_name.c_str(),
                      source_label);
            return false;
        }

        MaterialRecipeAsset *asset = files.Create(data, std::move(spec));
        if (!asset)
        {
            GLogError(u8"[MaterialRecipeRegistry] 无法分配配方记录: source=%ls", source_label);
            return false;
        }

        asset->SetSource(source_label);

        // 名字取注册时定下的值：`ResolvePending` 可能摘掉别的记录（含本条的失败态），
        // 之后指针不再可信，只按名字回查。
        const AnsiString recipe_name = asset->GetAssetName();

        ResolvePending(texture_manager);

        const MaterialRecipeAsset *resolved = FindAssetByName(recipe_name.c_str());
        if (!resolved)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方不可用（已摘除）: source=%ls recipe=%s",
                      source_label, recipe_name.c_str());
            return false;
        }

        if (!resolved->IsReady())
        {
            // 有 `parent` 但父配方还没注册。留在表里，等父配方就绪后由多轮推进
            // 或 `FinalizePending` 收尾（见头文件「加载顺序与差量继承」）。
            const AnsiString &parent_text = resolved->GetSpec().parent;
            GLogWarning(u8"[MaterialRecipeRegistry] 配方等待父配方: source=%ls recipe=%s parent=%s",
                        source_label,
                        recipe_name.c_str(),
                        parent_text.IsEmpty() ? "" : parent_text.c_str());
        }

        return true;
    }

    int MaterialRecipeRegistry::ResolveOne(
        MaterialRecipeAsset &asset,
        hgl::graph::TextureManager *texture_manager)
    {
        const MaterialRecipeSpec &own_spec = asset.GetSpec();
        const char *parent_text = own_spec.parent.c_str();

        if (parent_text && parent_text[0])
        {
            MaterialRecipeParentRef parent_ref;
            if (!ParseMaterialRecipeParentRef(parent_text, parent_ref) || parent_ref.IsNone())
            {
                GLogError(u8"[MaterialRecipeRegistry] 父配方引用非法: recipe=%s parent=%s",
                          own_spec.name.c_str(), parent_text);
                asset.MarkFailed("parent 引用写法非法");
                return -1;
            }

            const MaterialRecipeAsset *parent =
                (parent_ref.kind == MaterialRecipeParentRefKind::ByPath)
                    ? FindAssetByPath(parent_ref.path)
                    : FindAssetByName(parent_ref.name.c_str());

            if (!parent || !parent->IsReady())
                return 0;   // 父配方还没就绪（可能根本不存在，收尾时统一判定）

            MaterialRecipeSpec merged;
            MergeMaterialRecipeSpec(parent->GetSpec(), own_spec, merged);

            if (!asset.ApplyInheritedSpec(std::move(merged)))
            {
                GLogError(u8"[MaterialRecipeRegistry] 合并父配方后无法构建配方: recipe=%s parent=%s",
                          own_spec.name.c_str(), parent_text);
                asset.MarkFailed("合并父配方后无法构建运行时配方");
                return -1;
            }
        }

        if (!ResolveAndValidate(asset, texture_manager))
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方校验失败: recipe=%s domain=%s",
                      asset.GetRecipe().recipe_name.c_str(),
                      GetMaterialDomainName(asset.GetRecipe().domain));
            asset.MarkFailed("纹理槽与领域契约不符");
            return -1;
        }

        asset.MarkReady();
        return 1;
    }

    int MaterialRecipeRegistry::ResolvePending(hgl::graph::TextureManager *texture_manager)
    {
        int ready_count = 0;
        bool progressed = true;

        while (progressed)
        {
            progressed = false;

            for (int i = 0; i < files.GetCount(); ++i)
            {
                MaterialRecipeAsset *asset = files[i];
                if (!asset || asset->GetState() != MaterialRecipeAssetState::Parsed)
                    continue;

                if (ResolveOne(*asset, texture_manager) > 0)
                {
                    ++ready_count;
                    progressed = true;
                }
            }
        }

        // 失败的句柄立刻摘掉：表里只留「已就绪」和「等父配方」两种，
        // `FindAssetByName` 因此永远不会返回一个用不了的配方。
        for (int i = files.GetCount() - 1; i >= 0; --i)
        {
            const MaterialRecipeAsset *asset = files[i];
            if (asset && asset->GetState() == MaterialRecipeAssetState::Failed)
                files.DeleteAt(i);
        }

        return ready_count;
    }

    bool MaterialRecipeRegistry::HasParentCycle(const MaterialRecipeAsset &asset) const
    {
        // 沿 `parent` 走，最多走「表内记录数 + 1」步：无环链的长度不可能超过记录数，
        // 走满即说明必有重复 —— 不需要额外的 visited 集合。
        const int step_limit = files.GetCount() + 1;
        const MaterialRecipeAsset *cursor = &asset;

        for (int step = 0; step < step_limit; ++step)
        {
            const MaterialRecipeSpec &spec = cursor->GetSpec();
            const char *parent_text = spec.parent.c_str();
            if (!parent_text || !parent_text[0])
                return false;

            MaterialRecipeParentRef parent_ref;
            if (!ParseMaterialRecipeParentRef(parent_text, parent_ref) || parent_ref.IsNone())
                return false;

            cursor = (parent_ref.kind == MaterialRecipeParentRefKind::ByPath)
                         ? FindAssetByPath(parent_ref.path)
                         : FindAssetByName(parent_ref.name.c_str());

            if (!cursor)
                return false;   // 父不存在 ≠ 成环
        }

        return true;
    }

    int MaterialRecipeRegistry::FinalizePending()
    {
        int failed_count = 0;

        // 先只标记、不摘除：环判定要能看见完整的父链，摘除会让「成环」退化成「缺失」。
        for (int i = 0; i < files.GetCount(); ++i)
        {
            MaterialRecipeAsset *asset = files[i];
            if (!asset || asset->GetState() != MaterialRecipeAssetState::Parsed)
                continue;

            const bool cycle = HasParentCycle(*asset);
            const AnsiString &parent_text = asset->GetSpec().parent;
            GLogError(u8"[MaterialRecipeRegistry] 配方无法继承: recipe=%s parent=%s reason=%s",
                      asset->GetAssetName().c_str(),
                      parent_text.IsEmpty() ? "" : parent_text.c_str(),
                      cycle ? "父链成环" : "父配方缺失或解析失败");

            asset->MarkFailed(cycle ? "父链成环" : "父配方缺失或解析失败");
            ++failed_count;
        }

        for (int i = files.GetCount() - 1; i >= 0; --i)
        {
            const MaterialRecipeAsset *asset = files[i];
            if (asset && asset->GetState() == MaterialRecipeAssetState::Failed)
                files.DeleteAt(i);
        }

        return failed_count;
    }

    const MaterialRecipeAsset *MaterialRecipeRegistry::FindAssetByName(
        const char *recipe_name) const
    {
        if (!recipe_name || !recipe_name[0])
            return nullptr;

        for (int i = 0; i < files.GetCount(); ++i)
        {
            const MaterialRecipeAsset *asset = files[i];
            if (asset && asset->GetRecipe().recipe_name == recipe_name)
                return asset;
        }
        return nullptr;
    }

    const MaterialRecipeAsset *MaterialRecipeRegistry::FindAssetByPath(
        const OSString &path) const
    {
        if (path.IsEmpty())
            return nullptr;

        for (int i = 0; i < files.GetCount(); ++i)
        {
            const MaterialRecipeAsset *asset = files[i];
            if (asset && !asset->GetSourcePath().IsEmpty()
             && asset->GetSourcePath() == path)
                return asset;
        }
        return nullptr;
    }

    const MaterialRecipe *MaterialRecipeRegistry::FindByName(
        const char *recipe_name) const
    {
        const MaterialRecipeAsset *asset = FindAssetByName(recipe_name);

        // 等待父配方的句柄（`Parsed`）还没有可信的运行时配方，不能对外给出。
        if (!asset || !asset->IsReady())
            return nullptr;

        return &asset->GetRecipe();
    }
}
