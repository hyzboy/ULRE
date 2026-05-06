#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>
#include <memory>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_FLOAT, VAN::Luminance },
    };

    const UBOSemanticSet VERTEX_LUMINANCE_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet VERTEX_LUMINANCE_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef VERTEX_LUMINANCE_3D_DEF {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_3D_VERTEX,
        uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX) / sizeof(VERTEX_LUMINANCE_3D_VERTEX[0])),
        &VERTEX_LUMINANCE_3D_UBOS,
        &VERTEX_LUMINANCE_3D_SSBOS,
        nullptr,
        ShaderDataSchema::Color4f,
    };
}

std::unique_ptr<MaterialCreateInfo> CreateVertexLuminance3DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                                 Material3DCreateConfig *cfg,
                                                                 const MaterialVariantDesc &desc,
                                                                 const MaterialVariantKey &key)
{
    cfg->material_instance=true;

    return CreateFromFixedDef3DOwned("VertexLuminance3D", profile, VERTEX_LUMINANCE_3D_DEF, key, cfg, desc);
}

static std::unique_ptr<MaterialCreateInfo> VertexLuminance3D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateVertexLuminance3DOwned(profile, static_cast<Material3DCreateConfig *>(cfg), *desc, key); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexLuminance3D, "VertexLuminance3D", hgl::graph::mtl::VertexLuminance3D_Adapter)
