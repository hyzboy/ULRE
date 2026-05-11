#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl
{
    struct BoundRowSampleVariant
    {
        MaterialVariantKey key;
        MaterialVariantDesc desc;
    };

    MaterialVariantDesc BuildBoundRowSampleVariantDesc()
    {
        return CreateBuiltinRowBoundVariantDesc("StandardPBRArray", MaterialPreset::Standard);
    }

    BoundRowSampleVariant BuildBoundRowSampleVariant()
    {
        BoundRowSampleVariant sample;

        RuntimeKeyOverrides ov;
        ov.lighting_model = LightingModel::PBR;

        sample.key = RouteKey(MaterialPreset::Standard, VertexAttribFeatureBit(VertexAttrib::TexCoord), ov);
        sample.desc = BuildBoundRowSampleVariantDesc();
        return sample;
    }
}
