#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/PositionProviderRegistry.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/MaterialVariantDesc.h>

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

    const bool use_mesh_stage =
        (local_cfg.shader_stage_flag_bit & uint32_t(ShaderStage::Mesh)) != 0;

    std::string stage_glsl;

    if (use_mesh_stage)
    {
        // Mesh-path fullscreen triangle: emit clip-space vertices directly,
        // avoiding gl_VertexIndex-based providers that are vertex-stage-only.
        std::ostringstream mesh_out;
        mesh_out << "#version 460\n";
        mesh_out << "#extension GL_EXT_mesh_shader : require\n\n";
        mesh_out << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
        mesh_out << "layout(triangles, max_vertices = 3, max_primitives = 1) out;\n\n";
        mesh_out << "void main()\n";
        mesh_out << "{\n";
        mesh_out << "    SetMeshOutputsEXT(3u, 1u);\n";
        mesh_out << "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n";
        mesh_out << "    gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n";
        mesh_out << "    gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n";
        mesh_out << "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n";
        mesh_out << "}\n";
        stage_glsl = mesh_out.str();
    }
    else
    {
        // Build vertex-stage fullscreen triangle via PCG provider.
        const PositionProvider *pp = FindBuiltinProvider(PositionProviderId::PCG_FullscreenTriangle);
        std::ostringstream vs_out;
        vs_out << "#version 450\n\n";
        EmitPositionInput(vs_out, *pp, 0);
        vs_out << "\nvoid main()\n{\n    gl_Position = vec4(GetPositionLocal(), 1.0);\n}\n";
        stage_glsl = vs_out.str();
    }

    return CompileCompositorMaterial(profile,
                                     FULLSCREEN_TRIANGLE_DEF,
                                     stage_glsl,
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
