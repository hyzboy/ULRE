#pragma once

#include <hgl/mtl/MaterialDef.h>
#include <optional>
#include <string>

namespace hgl::graph::mtl
{
    /// Parse a MaterialDef from a .mat JSON file on disk.
    /// Returns std::nullopt on parse failure; error details are written to stderr.
    std::optional<MaterialDef> ParseMaterialDef(const std::string& file_path);

    /// Parse a MaterialDef from an in-memory JSON string.
    std::optional<MaterialDef> ParseMaterialDefFromString(const std::string& json_text);

} // namespace hgl::graph::mtl
