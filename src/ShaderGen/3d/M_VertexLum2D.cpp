#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_LUMINANCE_2D_VERTEX[] = {
        { VAT_VEC2, VAN::Position },
        { VAT_FLOAT, VAN::Luminance },
    };

    const UBOSemanticSet VERTEX_LUMINANCE_2D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet VERTEX_LUMINANCE_2D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef VERTEX_LUMINANCE_2D_DEF {
        "VertexLuminance2D",
        PrimitiveType::Lines,
        VERTEX_LUMINANCE_2D_VERTEX,
        uint32_t(sizeof(VERTEX_LUMINANCE_2D_VERTEX) / sizeof(VERTEX_LUMINANCE_2D_VERTEX[0])),
        &VERTEX_LUMINANCE_2D_UBOS,
        &VERTEX_LUMINANCE_2D_SSBOS,
        nullptr,
        ShaderDataSchema::Color4f,
    };
}

MaterialCreateInfo *CreateVertexLuminance2D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg,
                                            const MaterialVariantDesc &desc, const MaterialVariantKey &key)
{
    cfg->material_instance=true;

    StaticMaterialDef def = VERTEX_LUMINANCE_2D_DEF;
    if (cfg)
        def.primitive_type = cfg->prim;

    std::fprintf(stderr,
                 "[VertexLuminance2D] route primitive=%u va_bits=0x%08X\n",
                 static_cast<unsigned>(def.primitive_type),
                 key.vertex_attribute_feature_bits);

    MaterialVariantKey local_key = key;
    local_key.position_provider = PositionProviderId::SSBO_PackedVec2;

    return CreateFromFixedDef3D("VertexLuminance2D", profile, def, local_key, cfg, desc);
}

static MaterialCreateInfo *VertexLuminance2D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateVertexLuminance2D(profile, static_cast<Material3DCreateConfig *>(cfg), *desc, key); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexLuminance2D, "VertexLuminance2D", hgl::graph::mtl::VertexLuminance2D_Adapter)
