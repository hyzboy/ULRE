#include"FixedDefFactory3D.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry SKY_MINIMAL_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
    };

    const FixedUBODescriptors SKY_MINIMAL_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::SkyInfo,      uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT)},
    };

    const FixedSSBODescriptors SKY_MINIMAL_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,  uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
    };

    const FixedMaterialDef SKY_MINIMAL_DEF {
        "SkyMinimal",
        PrimitiveType::Triangles,
        SKY_MINIMAL_VERTEX,
        uint32_t(sizeof(SKY_MINIMAL_VERTEX) / sizeof(SKY_MINIMAL_VERTEX[0])),
        &SKY_MINIMAL_UBOS,
        &SKY_MINIMAL_SSBOS,
        nullptr,
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateSkyMinimal(const contract::PhysicalDeviceProfileLite *profile, const SkyMinimalCreateConfig *cfg)
{
    MaterialVariantKey var_key;
    var_key.surface_type = SurfaceType::Sky;
    return CreateFromFixedDef3D("SkyMinimal", profile, SKY_MINIMAL_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl