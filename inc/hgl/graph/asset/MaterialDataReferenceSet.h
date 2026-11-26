#pragma once

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    struct HGL_ASSET_STRUCT MaterialDataReference
    {
        OSString uri;       ///< Stable material data asset URI, never a device row.
        uint32 row = 0;     ///< Row within the referenced asset.
    };

    struct HGL_ASSET_STRUCT MaterialDataReferenceSet
    {
        uint32 schema = 1;
        ManagedArray<MaterialDataReference> instances; ///< Indexed by original instance index; duplicates are valid.
    };
}
