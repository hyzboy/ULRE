#pragma once

#include <hgl/graph/asset/MaterialDataReferenceSet.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    class MaterialDataBlockAsset;
    class MaterialDataBlockRegistry;

    struct ResolvedMaterialDataReference
    {
        MaterialDataBlockAsset *asset = nullptr;
        uint32 row = 0;
    };

    // Pointers are borrowed from registry and valid only while its assets remain alive.
    // Failure leaves out unchanged. The registry may load assets while resolving.
    bool ResolveMaterialDataReferenceSet(const MaterialDataReferenceSet &set,
                                         MaterialDataBlockRegistry &registry,
                                         ValueArray<ResolvedMaterialDataReference> &out,
                                         AnsiString &error);
}
