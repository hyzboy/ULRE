#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>
#include <hgl/mtl/FixedVertexEntry.h>

#include <cstdio>
#include <set>
#include <string>

using namespace hgl::graph::mtl;
using namespace hgl::graph;

namespace
{
    static constexpr FixedVertexEntry kVertexEntries[] = {
        { VAT_VEC3, VAN::Position }
    };

    static const std::string kVS =
        "#version 450\n"
        "layout(location=0) in vec3 inPosition;\n"
        "void main(){ gl_Position = vec4(inPosition, 1.0); }\n";

    static const std::string kFS =
        "#version 450\n"
        "layout(location=0) out vec4 outColor;\n"
        "void main(){ outColor = vec4(1.0); }\n";
}

int main()
{
    std::fprintf(stdout, "[CompositorInferenceDiagnosticsTest] Starting Phase 6 diagnostics-only validation...\n\n");

    UBOSemanticSet ubo_descriptors;
    AddUBODescriptor(ubo_descriptors, UBODescriptorSemantic::CameraInfo);
    AddUBODescriptor(ubo_descriptors, UBODescriptorSemantic::SkyInfo);

    StaticMaterialDef def{};
    def.name = "phase6_diag_material";
    def.primitive_type = PrimitiveType::Triangles;
    def.vertex_entries = kVertexEntries;
    def.vertex_entry_count = 1;
    def.ubo_descriptors = &ubo_descriptors;

    std::string out_vs;
    std::string out_fs;
    std::string diagnostics;

    const bool ok = PrepareCompositorGLSLForReflection(def,
                                                       kVS,
                                                       kFS,
                                                       out_vs,
                                                       out_fs,
                                                       &diagnostics);

    std::fprintf(stdout, "  Reflection prepare success: %s\n", ok ? "yes" : "no");
    std::fprintf(stdout, "  Diagnostics empty: %s\n", diagnostics.empty() ? "yes" : "no");

    const bool has_camera_diag = diagnostics.find("inferred camera=true differs") != std::string::npos;
    const bool has_sky_diag = diagnostics.find("inferred sky=true differs from configured/effective=false") != std::string::npos;

    std::fprintf(stdout, "  Camera mismatch diagnostic: %s\n", has_camera_diag ? "yes" : "no");
    std::fprintf(stdout, "  Sky mismatch diagnostic: %s\n", has_sky_diag ? "yes" : "no");

    if (!ok)
    {
        std::fprintf(stdout, "[CompositorInferenceDiagnosticsTest] FAIL: PrepareCompositorGLSLForReflection should succeed\n");
        return 1;
    }

    if (has_camera_diag || !has_sky_diag)
    {
        std::fprintf(stdout, "[CompositorInferenceDiagnosticsTest] FAIL: Expected only sky mismatch diagnostics for reflection default config\n");
        return 1;
    }

    if (out_vs.empty() || out_fs.empty())
    {
        std::fprintf(stdout, "[CompositorInferenceDiagnosticsTest] FAIL: GLSL outputs should still be produced\n");
        return 1;
    }

    std::fprintf(stdout, "  Diagnostics:\n%s\n\n", diagnostics.c_str());
    std::fprintf(stdout, "[CompositorInferenceDiagnosticsTest] PASS\n");
    return 0;
}
