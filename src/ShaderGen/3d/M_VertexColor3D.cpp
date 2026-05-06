#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include <memory>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC4, VAN::Color },
    };

    const UBOSemanticSet VERTEX_COLOR_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet VERTEX_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef VERTEX_COLOR_3D_DEF {
        "VertexColor3D",
        PrimitiveType::Triangles,
        VERTEX_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_COLOR_3D_VERTEX) / sizeof(VERTEX_COLOR_3D_VERTEX[0])),
        &VERTEX_COLOR_3D_UBOS,
        &VERTEX_COLOR_3D_SSBOS,
        nullptr,
        ShaderDataSchema::None
    };
}

std::unique_ptr<MaterialCreateInfo> CreateVertexColor3DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                             const Material3DCreateConfig *cfg,
                                                             const MaterialVariantDesc &desc,
                                                             const MaterialVariantKey &key)
{
    return CreateFromFixedDef3DOwned("VertexColor3D", profile, VERTEX_COLOR_3D_DEF, key, cfg, desc);
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY_FROM_OWNED(
    VertexColor3D,
    const hgl::graph::mtl::Material3DCreateConfig)
