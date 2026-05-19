#include"MaterialFactory3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_PALETTE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_UINT, VAN::Color },
    };

    const StaticMaterialDef VERTEX_PALETTE_COLOR_3D_DEF {
        "VertexPaletteColor3D",
        PrimitiveType::Triangles,
        VERTEX_PALETTE_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_PALETTE_COLOR_3D_VERTEX) / sizeof(VERTEX_PALETTE_COLOR_3D_VERTEX[0])),
        nullptr, nullptr, nullptr,
        ShaderDataSchema::None
    };
    static MaterialCreateInfo *CreateVertexPaletteColor3DFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &key,
        MaterialCreateConfig                      *cfg)
    {
        auto *cfg_3d = static_cast<const Material3DCreateConfig *>(cfg);
        Material3DCreateConfig local_cfg = cfg_3d ? *cfg_3d : Material3DCreateConfig();

        return CreateFromFixedDef3D("VertexPaletteColor3D", profile, VERTEX_PALETTE_COLOR_3D_DEF, key, &local_cfg, *desc);
    }
}//namespace
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexPaletteColor3D, "VertexPaletteColor3D", hgl::graph::mtl::CreateVertexPaletteColor3DFactory)
