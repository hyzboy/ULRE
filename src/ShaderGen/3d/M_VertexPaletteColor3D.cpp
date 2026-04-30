#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_PALETTE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_UINT, VAN::Color },
    };

    const UBOSemanticSet VERTEX_PALETTE_COLOR_3D_UBOS = []()
    {
        UBOSemanticSet descriptors = build3d::MakeViewportCameraUBOs();
        descriptors.insert(UBODescriptorSemantic::ColorPalette);
        return descriptors;
    }();

    const SSBOSemanticSet VERTEX_PALETTE_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef VERTEX_PALETTE_COLOR_3D_DEF {
        "VertexPaletteColor3D",
        PrimitiveType::Triangles,
        VERTEX_PALETTE_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_PALETTE_COLOR_3D_VERTEX) / sizeof(VERTEX_PALETTE_COLOR_3D_VERTEX[0])),
        &VERTEX_PALETTE_COLOR_3D_UBOS,
        &VERTEX_PALETTE_COLOR_3D_SSBOS,
        nullptr,
        ShaderDataSchema::None
    };
}//namespace

MaterialCreateInfo *CreateVertexPaletteColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg,
                                               const MaterialVariantDesc &desc, const MaterialVariantKey &key)
{
    Material3DCreateConfig local_cfg = build3d::MakeLocalConfig(cfg);

    return CreateFromFixedDef3D("VertexPaletteColor3D", profile, VERTEX_PALETTE_COLOR_3D_DEF, key, &local_cfg, desc);
}

static MaterialCreateInfo *VertexPaletteColor3D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateVertexPaletteColor3D(profile, static_cast<const Material3DCreateConfig *>(cfg), *desc, key); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexPaletteColor3D, "VertexPaletteColor3D", hgl::graph::mtl::VertexPaletteColor3D_Adapter)
