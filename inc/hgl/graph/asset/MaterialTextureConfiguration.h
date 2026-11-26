#pragma once

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    struct HGL_ASSET_STRUCT MaterialTextureAssetReference
    {
        OSString uri;  ///< Empty means an unbound slot.
        uint32 array_layer = 0;
        bool auto_mipmaps = false;
    };

    struct HGL_ASSET_STRUCT MaterialTextureConfiguration
    {
        uint32 schema = 1;
        ManagedArray<MaterialTextureAssetReference> textures; ///< Complete list in recipe layout order.
    };
}
