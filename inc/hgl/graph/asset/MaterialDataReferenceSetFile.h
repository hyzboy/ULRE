#pragma once

#include <hgl/graph/asset/MaterialDataReferenceSet.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    bool EncodeMaterialDataReferenceSet(const MaterialDataReferenceSet &set,
                                       ValueArray<uint8> &out,
                                       AnsiString &error);
    bool DecodeMaterialDataReferenceSet(const void *data, size_t size,
                                       MaterialDataReferenceSet &out,
                                       AnsiString &error);
    bool LoadMaterialDataReferenceSet(const OSString &path,
                                      MaterialDataReferenceSet &out,
                                      AnsiString &error);
    // Creates a new file only; replacing an existing asset requires a safe update path.
    bool CreateMaterialDataReferenceSetFile(const OSString &path,
                                            const MaterialDataReferenceSet &set,
                                            AnsiString &error);
}
