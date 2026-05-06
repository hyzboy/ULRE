#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/PositionProviderRegistry.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/MaterialVariantDesc.h>

#include <memory>
#include <sstream>
#include <string>

namespace hgl::graph::mtl
{
namespace
{
    // No vertex entries — positions are generated from gl_VertexIndex; no VBO binding needed.
    const StaticMaterialDef FULLSCREEN_TRIANGLE_DEF {
        "FullscreenTriangle",
        PrimitiveType::Triangles,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        ShaderDataSchema::None,
    };

    // PCG fullscreen triangle: (-1,-1), (3,-1), (-1,3) covers the entire NDC square.
    // VS is generated via EmitPositionInput(PCG_FullscreenTriangle) + trivial main.

    static const std::string kFullscreenTriangleFS =
        "#version 450\n"
        "\n"
        "layout(location = 0) out vec4 outColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    // Visualize window-space fragment coordinate directly.\n"
        "    outColor = vec4(fract(gl_FragCoord.xyz * 0.01), 1.0);\n"
        "}\n";
}

std::unique_ptr<MaterialCreateInfo> CreateFullscreenTriangleOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                                  Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    // PCG: no camera transform, no local-to-world, no material instance — purely procedural.
    local_cfg.camera              = false;
    local_cfg.sky                 = false;
    local_cfg.local_to_world      = false;
    local_cfg.material_instance   = false;
    local_cfg.effective_feature_mask = 0;

    // Build VS via PCG_FullscreenTriangle provider: no VAB, GetPositionLocal() → NDC.
    const PositionProvider *pp = FindBuiltinProvider(PositionProviderId::PCG_FullscreenTriangle);
    std::ostringstream vs_out;
    vs_out << "#version 450\n\n";
    EmitPositionInput(vs_out, *pp, 0);
    vs_out << "\nvoid main()\n{\n    gl_Position = vec4(GetPositionLocal(), 1.0);\n}\n";

    return CompileCompositorMaterialOwned(profile,
                                          FULLSCREEN_TRIANGLE_DEF,
                                          vs_out.str(),
                                          kFullscreenTriangleFS,
                                          &local_cfg);
}

MaterialCreateInfo *CreateFullscreenTriangle(const contract::PhysicalDeviceProfileLite *profile,
                                             Material3DCreateConfig *cfg)
{
    return CreateFullscreenTriangleOwned(profile,cfg).release();
}

static MaterialCreateInfo *FullscreenTriangle_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *,
    const MaterialVariantKey                  &,
    MaterialCreateConfig *cfg)
{ return CreateFullscreenTriangle(profile, static_cast<Material3DCreateConfig *>(cfg)); }

} // namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(FullscreenTriangle, "FullscreenTriangle", hgl::graph::mtl::FullscreenTriangle_Adapter)
