#pragma once

#include <hgl/mtl/MaterialPresetDef.h>
#include <optional>
#include <string>

namespace hgl::graph::mtl
{
    /// Parse a MaterialPresetDef from a .matpreset JSON file on disk.
    /// Returns std::nullopt on parse failure; error details are written to stderr.
    std::optional<MaterialPresetDef> ParseMaterialPresetDef(const std::string& file_path);

    /// Parse a MaterialPresetDef from an in-memory JSON string.
    std::optional<MaterialPresetDef> ParseMaterialPresetDefFromString(const std::string& json_text);

} // namespace hgl::graph::mtl
