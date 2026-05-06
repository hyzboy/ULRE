#include "StandardStaticDef.h"

#include <hgl/mtl/UBOCommon.h>

namespace hgl::graph::mtl
{
namespace
{

constexpr FixedVertexEntry kStandardVertex[] = {
    { VAT_VEC3, VAN::Position },
    { VAT_VEC2, VAN::TexCoord },
    { VAT_VEC3, VAN::Normal },
};

const UBOSemanticSet kStandardBaseUBOs = {
    UBODescriptorSemantic::ViewportInfo,
    UBODescriptorSemantic::CameraInfo,
    UBODescriptorSemantic::SkyInfo,
};

const SSBOSemanticSet kStandardBaseSSBOs = {
    SSBODescriptorSemantic::TransformData,
    SSBODescriptorSemantic::TransformID,
    SSBODescriptorSemantic::MaterialBindingInstanceID,
    SSBODescriptorSemantic::MaterialBindingInstanceData,
};

const SSBOSemanticSet kStandardArraySSBOs = {
    SSBODescriptorSemantic::TransformData,
    SSBODescriptorSemantic::TransformID,
    SSBODescriptorSemantic::MaterialBindingInstanceID,
    SSBODescriptorSemantic::MaterialBindingInstanceData,
    SSBODescriptorSemantic::MaterialBindingInstanceTexture,
};

const StaticTextureSamplerDescriptors kStandardSamplers2D = {
    { SamplerSlot::BaseColor, { SamplerType::Sampler2D, 0, 0, TextureChannelHint::RGBA } },
    { SamplerSlot::Normal,    { SamplerType::Sampler2D, 0, 0, TextureChannelHint::RGBA } },
};

const StaticTextureSamplerDescriptors kStandardSamplers2DArray = {
    { SamplerSlot::BaseColor, { SamplerType::Sampler2DArray, 0, 0, TextureChannelHint::RGBA } },
    { SamplerSlot::Normal,    { SamplerType::Sampler2DArray, 0, 0, TextureChannelHint::RGBA } },
};

constexpr SamplerSlot kStandardTextureSlots[] = {
    SamplerSlot::BaseColor,
    SamplerSlot::Normal,
};

} // namespace

const SSBOSemanticSet &GetStandardBaseSSBOs() noexcept
{
    return kStandardBaseSSBOs;
}

const SamplerSlot *GetStandardTextureSlots(uint32_t &slot_count) noexcept
{
    slot_count = uint32_t(sizeof(kStandardTextureSlots) / sizeof(kStandardTextureSlots[0]));
    return kStandardTextureSlots;
}

StaticMaterialDef BuildCanonicalStandardStaticDef(const bool any_array) noexcept
{
    StaticMaterialDef def {
        any_array ? "StandardTextureArray_v1" : "Standard_v1",
        PrimitiveType::Triangles,
        kStandardVertex,
        uint32_t(sizeof(kStandardVertex) / sizeof(kStandardVertex[0])),
        &kStandardBaseUBOs,
        any_array ? &kStandardArraySSBOs : &kStandardBaseSSBOs,
        any_array ? &kStandardSamplers2DArray : &kStandardSamplers2D,
        ShaderDataSchema::StandardParams,
    };

    return def;
}

} // namespace hgl::graph::mtl
