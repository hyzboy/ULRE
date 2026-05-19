#include "MaterialFactory3DCommon.h"
#include <hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry PBR_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC3, VAN::Normal   },
    };

    const StaticMaterialDef PBR_COLOR_3D_DEF {
        "PBRColor3D",
        PrimitiveType::Triangles,
        PBR_COLOR_3D_VERTEX,
        uint32_t(sizeof(PBR_COLOR_3D_VERTEX) / sizeof(PBR_COLOR_3D_VERTEX[0])),
        nullptr, nullptr, nullptr,
        ShaderDataSchema::PBRColorParams,
    };

    static MaterialCreateInfo *CreatePBRColor3DFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &key,
        MaterialCreateConfig                      *cfg)
    {
        auto *pbr_cfg = static_cast<PBRColor3DMaterialCreateConfig *>(cfg);
        if (pbr_cfg)
            pbr_cfg->material_instance = true;

        return CreateFromFixedDef3D("PBRColor3D", profile, PBR_COLOR_3D_DEF, key,
                                    static_cast<const Material3DCreateConfig *>(cfg), *desc);
    }
}//namespace
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PBRColor3D, "PBRColor3D", hgl::graph::mtl::CreatePBRColor3DFactory)
