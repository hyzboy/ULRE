#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/MaterialVariantDesc.h>

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
    static const std::string kFullscreenTriangleVS =
        "#version 450\n"
        "\n"
        "const vec2 kPos[3] = vec2[3](\n"
        "    vec2(-1.0, -1.0),\n"
        "    vec2( 3.0, -1.0),\n"
        "    vec2(-1.0,  3.0)\n"
        ");\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(kPos[gl_VertexIndex], 0.0, 1.0);\n"
        "}\n";

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

MaterialCreateInfo *CreateFullscreenTriangle(const contract::PhysicalDeviceProfileLite *profile,
                                             Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    // PCG: no camera transform, no local-to-world, no material instance — purely procedural.
    local_cfg.camera              = false;
    local_cfg.sky                 = false;
    local_cfg.local_to_world      = false;
    local_cfg.material_instance   = false;
    local_cfg.effective_feature_mask = 0;

    return CompileCompositorMaterial(profile,
                                     FULLSCREEN_TRIANGLE_DEF,
                                     kFullscreenTriangleVS,
                                     kFullscreenTriangleFS,
                                     &local_cfg);
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
