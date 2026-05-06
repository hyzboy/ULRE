#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>
#include <memory>

namespace hgl::graph::mtl
{
namespace
{
    constexpr FixedVertexEntry GIZMO_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC3, VAN::Normal },
    };

    const UBOSemanticSet GIZMO_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet GIZMO_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef GIZMO_3D_DEF {
        "Gizmo3D",
        PrimitiveType::Triangles,
        GIZMO_3D_VERTEX,
        uint32_t(sizeof(GIZMO_3D_VERTEX) / sizeof(GIZMO_3D_VERTEX[0])),
        &GIZMO_3D_UBOS,
        &GIZMO_3D_SSBOS,
        nullptr,
        ShaderDataSchema::Color4f,
    };
}

std::unique_ptr<MaterialCreateInfo> CreateGizmo3DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                       Material3DCreateConfig *cfg,
                                                       const MaterialVariantDesc &desc,
                                                       const MaterialVariantKey &key)
{
    if(cfg)
        cfg->material_instance=true;    // Gizmo requires per-instance data

    return CreateFromFixedDef3DOwned("Gizmo3D", profile, GIZMO_3D_DEF, key, cfg, desc);
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY_FROM_OWNED(
    Gizmo3D,
    hgl::graph::mtl::Material3DCreateConfig)
