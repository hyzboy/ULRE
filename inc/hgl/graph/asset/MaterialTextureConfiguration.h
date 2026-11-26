#pragma once

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    inline constexpr uint32 MaterialTextureConfigurationSchema = 2;

    struct HGL_ASSET_STRUCT MaterialTextureAssetReference
    {
        OSString uri;  ///< Empty means an unbound slot.
        uint32 array_layer = 0;
        bool auto_mipmaps = false;
    };

    struct HGL_ASSET_STRUCT MaterialTextureConfiguration
    {
        uint32 schema = MaterialTextureConfigurationSchema;
        AnsiString id; ///< Persistent AssetID; not part of texture-content equality.
        ManagedArray<MaterialTextureAssetReference> textures; ///< Complete list in recipe layout order.
    };

    // This compares authored references only; a reusable GPU row also requires
    // an independently verified matching texture layout and device.
    inline bool HasSameTextureReferences(const MaterialTextureConfiguration &left,
                                         const MaterialTextureConfiguration &right)
    {
        if (left.schema != right.schema || left.textures.GetCount() != right.textures.GetCount())
            return false;

        for (int i = 0; i < left.textures.GetCount(); ++i)
        {
            const MaterialTextureAssetReference *a = left.textures.At(i);
            const MaterialTextureAssetReference *b = right.textures.At(i);
            if (!a || !b || a->uri != b->uri || a->array_layer != b->array_layer
                || a->auto_mipmaps != b->auto_mipmaps)
                return false;
        }
        return true;
    }
}
