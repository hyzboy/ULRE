#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantDesc.h>

#include <cstdio>
#include <filesystem>
#include <string>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
std::string ResolveShaderLibraryPath()
{
    namespace fs = std::filesystem;

    fs::path from_file = fs::path(__FILE__);
    if (!from_file.empty() && from_file.has_parent_path())
    {
        // __FILE__: .../src/Tools/ShaderGen/CompositorTemplatePathGuardTest.cpp
        fs::path root = from_file.parent_path().parent_path().parent_path().parent_path();
        fs::path shader_lib = root / "ShaderLibrary";
        return shader_lib.lexically_normal().generic_string();
    }

    // Fallback for unusual compile path setups.
    return (fs::current_path() / "../ShaderLibrary").lexically_normal().generic_string();
}

MaterialVariantKey Make2DKey()
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::VertexColor2D;
    key.geometry_mode = GeometryMode::Quad2D;
    key.pass_hint = PassType::ForwardOpaque;
    key.blend_mode = RenderAlphaMode::Opaque;
    key.SetVertexAttribEnabled(VertexAttrib::Position, true);
    key.SetVertexAttribEnabled(VertexAttrib::Color, true);
    return key;
}
} // anonymous namespace

int main()
{
    std::fprintf(stdout, "[CompositorTemplatePathGuardTest] Start\n");

    const std::string shader_lib = ResolveShaderLibraryPath();
    CompositorAssembler assembler(shader_lib);

    const MaterialVariantKey key = Make2DKey();

    // Case 1: non-compositor explicit templates must still allow disk path loading.
    {
        MaterialVariantDesc desc;
        desc.variant_name = "VertexColor2D";
        desc.vs_template_path = "2d/vertexcolor2d.vert.glsl";
        desc.fs_template_path = "2d/vertexcolor2d.frag.glsl";

        const auto result = assembler.Assemble(key, desc);
        if (!result.success)
        {
            std::fprintf(stderr,
                         "[CompositorTemplatePathGuardTest] FAIL: 2d explicit templates should be allowed, error=%s\n",
                         result.error_message.c_str());
            return 1;
        }

        if (result.vertex_glsl.empty() || result.fragment_glsl.empty())
        {
            std::fprintf(stderr,
                         "[CompositorTemplatePathGuardTest] FAIL: 2d explicit templates produced empty GLSL\n");
            return 1;
        }

        std::fprintf(stdout, "  Case1 OK: non-compositor explicit templates still load\n");
    }

    // Case 2: compositor/ prefixed template paths are now key-derived;
    //         the path string is ignored and assembly must succeed via key-derived generation.
    {
        MaterialVariantDesc desc;
        desc.variant_name     = "CompositorAliasIgnored";
        desc.vs_template_path = "compositor/not_a_real_file.vert.glsl";
        desc.fs_template_path = "2d/vertexcolor2d.frag.glsl";

        const auto result = assembler.Assemble(key, desc);
        if (!result.success)
        {
            std::fprintf(stderr,
                         "[CompositorTemplatePathGuardTest] FAIL: compositor/ prefix path should route to key-derived generation, error=%s\n",
                         result.error_message.c_str());
            return 1;
        }

        if (result.vertex_glsl.find("#version") == std::string::npos)
        {
            std::fprintf(stderr,
                         "[CompositorTemplatePathGuardTest] FAIL: key-derived VS should contain #version directive\n");
            return 1;
        }

        std::fprintf(stdout, "  Case2 OK: compositor/ prefix routes to key-derived generation\n");
    }

    std::fprintf(stdout, "[CompositorTemplatePathGuardTest] PASS\n");
    return 0;
}
