#pragma once

#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    enum class MaterialRecipeFileParseResult : uint8
    {
        Skipped = 0,
        OK,
        InvalidValue,
        UnknownKey
    };

    struct MaterialRecipeFileParseError
    {
        AnsiString table;
        AnsiString key;
        AnsiString value;
        AnsiString detail;
        int line = 0;

        bool IsEmpty() const noexcept
        {
            return line == 0 && table.IsEmpty() && key.IsEmpty()
                && value.IsEmpty() && detail.IsEmpty();
        }
    };

    struct MaterialRecipeFileData
    {
        MaterialRecipe recipe;
        MaterialRecipeFileParseError parse_error;
    };

    const char *GetMaterialRecipeFileParseResultName(
        MaterialRecipeFileParseResult result) noexcept;

    MaterialRecipeFileParseResult ParseMaterialRecipeFile(
        const char *content,
        int content_size,
        MaterialRecipeFileData &out_data,
        MaterialRecipeSpec *out_spec = nullptr) noexcept;

    /**
     * 由**已合并**的规约重建运行时配方。
     *
     * `ParseMaterialRecipeFile` 对无父配方会直接给出 `out_data.recipe`；对**有 `parent`
     * 的配方**它只填名字（差量文件允许缺 `domain` 等键，此时无法定型），真正的运行时
     * 配方要等 `MaterialRecipeRegistry` 做完差量合并后走本函数建出来。
     *
     * 失败时 `out_recipe` 内容未定义。
     */
    bool BuildMaterialRecipe(const MaterialRecipeSpec &spec,
                             MaterialRecipe &out_recipe) noexcept;
}
