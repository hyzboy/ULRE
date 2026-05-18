#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::mtl{
namespace
{
    // ── 3D vertex layout
    constexpr FixedVertexEntry kVL3DVertex[] = {
        { VAT_VEC3,  VAN::Position  },
        { VAT_FLOAT, VAN::Luminance },
    };
    const UBOSemanticSet  kVL3DUBOs  = build3d::MakeViewportCameraUBOs();
    const SSBOSemanticSet kVL3DSSBOs = build3d::MakeTransformSSBOs(true);
    const StaticMaterialDef kVL3DDef {
        "VertexLuminance", PrimitiveType::Triangles,
        kVL3DVertex, 2,
        &kVL3DUBOs, &kVL3DSSBOs, nullptr,
        ShaderDataSchema::Color4f,
    };

    // ── 2D vertex layout
    constexpr FixedVertexEntry kVL2DVertex[] = {
        { VAT_VEC2,  VAN::Position  },
        { VAT_FLOAT, VAN::Luminance },
    };
    const UBOSemanticSet  kVL2DUBOs  = build3d::MakeViewportCameraUBOs();
    const SSBOSemanticSet kVL2DSSBOs = build3d::MakeTransformSSBOs(true);

    // ── Unified adapter (both 2D and 3D share same FS; differ only in vertex transform policy)
    static MaterialCreateInfo *VertexLuminance_Adapter(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &key_in,
        MaterialCreateConfig                      *cfg)
    {
        auto *c3 = static_cast<Material3DCreateConfig *>(cfg);
        if (!profile || !c3) return nullptr;
        c3->material_instance = true;

        const bool is2D = (c3->position_format == VAT_VEC2);

        if (!is2D)
            return CreateFromFixedDef3D("VertexLuminance", profile, kVL3DDef, key_in, c3, *desc);

        // 2D path: build def with 2D vertex layout and override primitive from config
        StaticMaterialDef def2d {
            "VertexLuminance", c3->prim,
            kVL2DVertex, 2,
            &kVL2DUBOs, &kVL2DSSBOs, nullptr,
            ShaderDataSchema::Color4f,
        };
        MaterialVariantKey local_key = key_in;
        local_key.position_provider = PositionProviderId::VAB_Vec2;
        return CreateFromFixedDef3D("VertexLuminance", profile, def2d, local_key, c3, *desc);
    }
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexLuminance, "VertexLuminance", hgl::graph::mtl::VertexLuminance_Adapter)
