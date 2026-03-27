#include"FixedDefFactory3D.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char VERTEX_LUMINANCE_3D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_3D_MI_BYTES = sizeof(hgl::math::Vector4f);

    constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
        { VAT_FLOAT, VertexInputRate::Vertex, VAN::Luminance },
    };

    const FixedUBODescriptors VERTEX_LUMINANCE_3D_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedSSBODescriptors VERTEX_LUMINANCE_3D_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,     uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,      uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstance, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedMaterialDef VERTEX_LUMINANCE_3D_DEF {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_3D_VERTEX,
        uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX) / sizeof(VERTEX_LUMINANCE_3D_VERTEX[0])),
        &VERTEX_LUMINANCE_3D_UBOS,
        &VERTEX_LUMINANCE_3D_SSBOS,
        nullptr,
        VERTEX_LUMINANCE_3D_MI_CODES,
        VERTEX_LUMINANCE_3D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    MaterialVariantKey var_key;
    var_key.SetVertexAttribEnabled(VertexAttrib::Luminance);
    return CreateFromFixedDef3D("VertexLuminance3D", profile, VERTEX_LUMINANCE_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl