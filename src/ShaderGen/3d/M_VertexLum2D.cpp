#include"MaterialFactory3DCommon.h"
#include<hgl/log/Log.h>
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

    static MaterialCreateInfo *CreateVertexLuminance2DFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &key,
        MaterialCreateConfig                      *cfg)
    {
        auto *cfg_3d=static_cast<Material3DCreateConfig *>(cfg);
        cfg_3d->material_instance=true;

        StaticMaterialDef def = VERTEX_LUMINANCE_2D_DEF;
        if (cfg_3d)
            def.primitive_type = cfg_3d->prim;

        GLogError(
                     "[VertexLuminance2D] route primitive=%u va_bits=0x%08X\n",
                     static_cast<unsigned>(def.primitive_type),
                     key.vertex_attribute_feature_bits);

        MaterialVariantKey local_key = key;
        local_key.position_provider = PositionProviderId::VAB_Vec2;

        return CreateFromFixedDef3D("VertexLuminance2D", profile, def, local_key, cfg_3d, *desc);
    }
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexLuminance2D, "VertexLuminance2D", hgl::graph::mtl::CreateVertexLuminance2DFactory)
