#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include <memory>

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

std::unique_ptr<MaterialCreateInfo> CreateVertexPaletteColor3DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                                    const Material3DCreateConfig *cfg,
                                                                    const MaterialVariantDesc &desc,
                                                                    const MaterialVariantKey &key)
{
    Material3DCreateConfig local_cfg = build3d::MakeLocalConfig(cfg);

    return CreateFromFixedDef3DOwned("VertexPaletteColor3D", profile, VERTEX_PALETTE_COLOR_3D_DEF, key, &local_cfg, desc);
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY_FROM_OWNED(
    VertexPaletteColor3D,
    const hgl::graph::mtl::Material3DCreateConfig)
