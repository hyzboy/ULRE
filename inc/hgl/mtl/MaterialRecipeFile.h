#pragma once

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
        MaterialRecipeFileData &out_data) noexcept;
}
