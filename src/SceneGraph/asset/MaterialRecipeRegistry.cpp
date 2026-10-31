#include <hgl/mtl/MaterialRecipeRegistry.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>

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
        MaterialRecipeFileData &data,
        hgl::graph::TextureManager *texture_manager)
    {
        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID(data.recipe.mtl_def_id, definition))
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方引用了未知材质定义: recipe=%s definition=%s",
                      data.recipe.recipe_name.c_str(),
                      data.recipe.mtl_def_id.c_str());
            return false;
        }

        for (RecipeTextureBinding &binding : data.recipe.textures)
        {
            const int declaration_index =
                FindMaterialTextureDeclaration(definition, binding.texture_name);
            if (declaration_index < 0)
            {
                GLogError(u8"[MaterialRecipeRegistry] 配方纹理槽未在材质定义中声明: recipe=%s slot=%s definition=%s",
                          data.recipe.recipe_name.c_str(),
                          binding.texture_name.c_str(),
                          data.recipe.mtl_def_id.c_str());
                return false;
            }

            const MaterialTextureDeclaration &declaration =
                definition.texture_declarations[static_cast<size_t>(declaration_index)];
            if (binding.array_layer != 0
             && !IsMaterialTextureArraySampler(declaration.sampler_type))
            {
                GLogError(u8"[MaterialRecipeRegistry] 非数组纹理不能指定非零 layer: recipe=%s slot=%s layer=%u",
                          data.recipe.recipe_name.c_str(),
                          binding.texture_name.c_str(),
                          binding.array_layer);
                return false;
            }
        }

        if (!data.recipe.textures.empty() && !texture_manager)
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方声明纹理但没有 TextureManager: recipe=%s",
                      data.recipe.recipe_name.c_str());
            return false;
        }

        for (RecipeTextureBinding &binding : data.recipe.textures)
        {
            hgl::graph::TextureAsset *asset =
                texture_manager->CreateTextureAsset(
                    ToOSString(binding.resource_id.c_str()),
                    binding.auto_mipmaps);
            if (!asset)
            {
                GLogError(u8"[MaterialRecipeRegistry] 创建纹理句柄失败: recipe=%s slot=%s resource=%s",
                          data.recipe.recipe_name.c_str(),
                          binding.texture_name.c_str(),
                          binding.resource_id.c_str());
                return false;
            }

            if (!data.recipe.AddTexture(binding.texture_name,
                                        asset,
                                        binding.required,
                                        binding.array_layer))
            {
                GLogError(u8"[MaterialRecipeRegistry] 无法绑定纹理句柄: recipe=%s slot=%s",
                          data.recipe.recipe_name.c_str(),
                          binding.texture_name.c_str());
                return false;
            }
        }

        NormalizeRecipe(data.recipe);
        if (!IsRecipeNormalized(data.recipe))
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方规范化失败: recipe=%s definition=%s",
                      data.recipe.recipe_name.c_str(),
                      data.recipe.mtl_def_id.c_str());
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
        MaterialRecipeFileData *data = files.Create();
        if (!data)
        {
            GLogError(u8"[MaterialRecipeRegistry] 无法分配配方记录: source=%ls",
                      label ? label : OS_TEXT("<memory>"));
            return false;
        }

        const MaterialRecipeFileParseResult parse_result =
            ParseMaterialRecipeFile(content, content_size, *data);
        if (parse_result != MaterialRecipeFileParseResult::OK)
        {
            LogRecipeParseFailure(
                label ? label : OS_TEXT("<memory>"),
                parse_result,
                data->parse_error);
            files.DeleteAt(files.GetCount() - 1);
            return false;
        }

        for (int i = 0; i < files.GetCount() - 1; ++i)
        {
            const MaterialRecipeFileData *existing = files[i];
            if (existing
             && existing->recipe.recipe_name == data->recipe.recipe_name)
            {
                GLogError(u8"[MaterialRecipeRegistry] 配方名称重复: %s (source=%ls)",
                          data->recipe.recipe_name.c_str(),
                          label ? label : OS_TEXT("<memory>"));
                files.DeleteAt(files.GetCount() - 1);
                return false;
            }
        }

        if (!ResolveAndValidate(*data, texture_manager))
        {
            GLogError(u8"[MaterialRecipeRegistry] 配方校验失败: source=%ls recipe=%s definition=%s",
                      label ? label : OS_TEXT("<memory>"),
                      data->recipe.recipe_name.c_str(),
                      data->recipe.mtl_def_id.c_str());
            files.DeleteAt(files.GetCount() - 1);
            return false;
        }
        return true;
    }

    const MaterialRecipe *MaterialRecipeRegistry::FindByName(
        const char *recipe_name) const
    {
        if (!recipe_name || !recipe_name[0])
            return nullptr;

        for (int i = 0; i < files.GetCount(); ++i)
        {
            const MaterialRecipeFileData *data = files[i];
            if (data && data->recipe.recipe_name == recipe_name)
                return &data->recipe;
        }
        return nullptr;
    }
}
