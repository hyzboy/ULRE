#pragma once

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    enum class MaterialDefinitionFileParseResult : uint8
    {
        Skipped = 0,
        OK,
        InvalidValue,
        InvalidSemantic,
        InvalidNumber
    };

    const char *GetMaterialDefinitionFileParseResultName(
        MaterialDefinitionFileParseResult result) noexcept;

    struct MaterialDefinitionFileData
    {
        MaterialDefinition definition;
        AnsiString fragment_module_storage;
        AnsiString surface_module_storage;
    };

    MaterialDefinitionFileParseResult ParseMaterialDefinitionFile(
        const char *content,
        int content_size,
        MaterialDefinitionFileData &out_data) noexcept;

    bool IsValidMaterialDefinitionFileData(
        const MaterialDefinitionFileData &data) noexcept;

    class MaterialDefinitionFileRegistry
    {
        ManagedArray<MaterialDefinitionFileData> files;

    public:
        bool LoadFile(const OSString &path);
        bool LoadDirectory(const OSString &directory,
                           int *out_file_count = nullptr,
                           int *out_error_count = nullptr);

        const MaterialDefinition *FindByID(const char *definition_id) const;
        int GetCount() const { return files.GetCount(); }
        void Clear() { files.Clear(); }
    };
}
