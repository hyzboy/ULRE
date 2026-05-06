#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include <memory>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry PURE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    const UBOSemanticSet PURE_COLOR_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet PURE_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef PURE_COLOR_3D_DEF {
        "PureColor3D",
        PrimitiveType::Triangles,
        PURE_COLOR_3D_VERTEX,
        uint32_t(sizeof(PURE_COLOR_3D_VERTEX) / sizeof(PURE_COLOR_3D_VERTEX[0])),
        &PURE_COLOR_3D_UBOS,
        &PURE_COLOR_3D_SSBOS,
        nullptr,
        ShaderDataSchema::Color4f,
    };
}

std::unique_ptr<MaterialCreateInfo> CreatePureColor3DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                           Material3DCreateConfig *cfg,
                                                           const MaterialVariantDesc &desc,
                                                           const MaterialVariantKey &key)
{
    return CreateFromFixedDef3DOwned("PureColor3D", profile, PURE_COLOR_3D_DEF, key, cfg, desc);
}

static std::unique_ptr<MaterialCreateInfo> PureColor3D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreatePureColor3DOwned(profile, static_cast<Material3DCreateConfig *>(cfg), *desc, key); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PureColor3D, "PureColor3D", hgl::graph::mtl::PureColor3D_Adapter)
