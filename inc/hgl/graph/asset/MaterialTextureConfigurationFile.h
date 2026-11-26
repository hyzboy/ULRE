#pragma once

#include <hgl/graph/asset/MaterialTextureConfiguration.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    bool EncodeMaterialTextureConfiguration(const MaterialTextureConfiguration &configuration,
                                            ValueArray<uint8> &out, AnsiString &error);
    bool DecodeMaterialTextureConfiguration(const void *data, size_t size,
                                            MaterialTextureConfiguration &out, AnsiString &error);
    // Reads a physical path or an asset URI through the mounted asset sources.
    bool LoadMaterialTextureConfiguration(const OSString &path,
                                          MaterialTextureConfiguration &out, AnsiString &error);
    // Creates a new file only; replacing an existing asset requires a safe update path.
    bool CreateMaterialTextureConfigurationFile(const OSString &path,
                                                const MaterialTextureConfiguration &configuration,
                                                AnsiString &error);
}
