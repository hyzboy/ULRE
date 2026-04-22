#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/StaticMaterialDef.h>

#include <string>

namespace hgl::graph::mtl
{
namespace
{
    constexpr FixedVertexEntry CHECKERBOARD_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    const StaticMaterialDef CHECKERBOARD_3D_DEF {
        "Checkerboard3D",
        PrimitiveType::Triangles,
        CHECKERBOARD_3D_VERTEX,
        uint32_t(sizeof(CHECKERBOARD_3D_VERTEX) / sizeof(CHECKERBOARD_3D_VERTEX[0])),
        nullptr,
        nullptr,
        nullptr,
        ShaderDataSchema::None,
    };

    static const std::string kCheckerboardVS =
        "#version 450\n"
        "layout(location=0) in vec3 inPosition;\n"
        "layout(location=0) out vec3 vWorldPos;\n"
        "void main()\n"
        "{\n"
        "    vWorldPos = inPosition;\n"
        "    gl_Position = vec4(inPosition, 1.0);\n"
        "}\n";

    static const std::string kCheckerboardFS =
        "#version 450\n"
        "layout(location=0) in vec3 vWorldPos;\n"
        "layout(location=0) out vec4 outColor;\n"
        "void main()\n"
        "{\n"
        "    vec2 grid = floor(vWorldPos.xz * 0.5);\n"
        "    float checker = mod(grid.x + grid.y, 2.0);\n"
        "    vec3 c0 = vec3(0.65, 0.65, 0.65);\n"
        "    vec3 c1 = vec3(0.25, 0.25, 0.25);\n"
        "    vec3 col = mix(c0, c1, checker);\n"
        "    outColor = vec4(col, 1.0);\n"
        "}\n";
}

MaterialCreateInfo *CreateCheckerboard3D(const contract::PhysicalDeviceProfileLite *profile,
                                         Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    // Hard requirement: fallback checkerboard depends only on vertex position.
    local_cfg.camera = false;
    local_cfg.sky = false;
    local_cfg.local_to_world = false;
    local_cfg.material_instance = false;
    local_cfg.effective_feature_mask = 0;

    return CompileCompositorMaterial(profile,
                                     CHECKERBOARD_3D_DEF,
                                     kCheckerboardVS,
                                     kCheckerboardFS,
                                     &local_cfg);
}

static MaterialCreateInfo *Checkerboard3D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{ return CreateCheckerboard3D(profile, static_cast<Material3DCreateConfig *>(cfg)); }

} // namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Checkerboard3D, "Checkerboard3D", hgl::graph::mtl::Checkerboard3D_Adapter)
