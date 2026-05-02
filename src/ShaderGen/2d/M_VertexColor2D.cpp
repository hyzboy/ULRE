#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{

static bool UsesMeshPipeline(const Material2DCreateConfig &cfg)
{
    const uint32_t bits = cfg.shader_stage_flag_bit;
    const bool has_mesh = (bits & uint32_t(ShaderStage::Mesh)) != 0;
    const bool has_vertex = (bits & uint32_t(ShaderStage::Vertex)) != 0;
    return has_mesh && !has_vertex;
}

static std::string BuildVertexColor2DMeshShader(const Material2DCreateConfig *cfg,
                                                const MaterialVariantKey &key)
{
    if (!cfg)
        return std::string();

    if (key.position_provider != PositionProviderId::SSBO_PackedVec3)
        return std::string();

    const AttributeProviderId color_provider = key.attribute_providers[size_t(AttributeSemantic::Color)];
    if (color_provider != AttributeProviderId::SSBO_PackedRGBA8
     && color_provider != AttributeProviderId::SSBO_Vec4)
    {
        return std::string();
    }

    std::string glsl;
    glsl.reserve(4096);

    glsl += "#version 460\n";
    glsl += "#extension GL_EXT_mesh_shader : require\n\n";
    glsl += "#define ULRE_MESH_SHADER_STAGE 1\n";
    glsl += "#ifndef VERTEXSTREAMS_SET\n";
    glsl += "#define VERTEXSTREAMS_SET 4\n";
    glsl += "#endif\n\n";

    glsl += "#define POSITION_SSBO_SET VERTEXSTREAMS_SET\n";
    glsl += "#define POSITION_SSBO_BINDING ";
    glsl += std::to_string(size_t(AttributeSemantic::BuiltinCount));
    glsl += "\n";
    glsl += "#include \"position_provider/ssbo_packed.glsl\"\n\n";

    glsl += "#define ATTRIB_SET VERTEXSTREAMS_SET\n";
    glsl += "#define ATTRIB_BINDING ";
    glsl += std::to_string(size_t(AttributeSemantic::Color));
    glsl += "\n";
    glsl += "#define ATTRIB_TAG Color\n";

    if (color_provider == AttributeProviderId::SSBO_PackedRGBA8)
        glsl += "#include \"attribute_provider/ssbo_packed_rgba8.glsl\"\n";
    else
        glsl += "#include \"attribute_provider/ssbo_vec4.glsl\"\n";

    glsl += "#undef ATTRIB_TAG\n";
    glsl += "#undef ATTRIB_BINDING\n";
    glsl += "#undef ATTRIB_SET\n\n";

    if (cfg->coordinate_system == CoordinateSystem2D::Ortho)
        glsl += "#include \"common/ubo_viewport.glsl\"\n";

    if (cfg->local_to_world)
        glsl += "#include \"common/ssbo_transform.glsl\"\n";

    glsl += "\nlayout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
    glsl += "layout(triangles, max_vertices = 3, max_primitives = 1) out;\n\n";
    glsl += "layout(location = 0) out vec4 fragColor[];\n\n";

    glsl += "vec4 FetchColor(uint i)\n";
    glsl += "{\n";
    glsl += "    return ReadAttrib_Color(i);\n";
    glsl += "}\n\n";

    glsl += "vec4 ToClip(vec3 pos)\n";
    glsl += "{\n";

    if (cfg->coordinate_system == CoordinateSystem2D::Ortho && cfg->local_to_world)
        glsl += "    return GetTransform() * viewport.ortho_matrix * vec4(pos.xy, 0.0, 1.0);\n";
    else if (cfg->coordinate_system == CoordinateSystem2D::Ortho)
        glsl += "    return viewport.ortho_matrix * vec4(pos.xy, 0.0, 1.0);\n";
    else if (cfg->coordinate_system == CoordinateSystem2D::ZeroToOne && cfg->local_to_world)
        glsl += "    return GetTransform() * vec4(pos.xy * 2.0 - 1.0, 0.0, 1.0);\n";
    else if (cfg->coordinate_system == CoordinateSystem2D::ZeroToOne)
        glsl += "    return vec4(pos.xy * 2.0 - 1.0, 0.0, 1.0);\n";
    else if (cfg->local_to_world)
        glsl += "    return GetTransform() * vec4(pos.xy, 0.0, 1.0);\n";
    else
        glsl += "    return vec4(pos.xy, 0.0, 1.0);\n";

    glsl += "}\n\n";

    glsl += "void main()\n";
    glsl += "{\n";
    glsl += "    SetMeshOutputsEXT(3u, 1u);\n";
    glsl += "\n";
    glsl += "    const uint tri = gl_WorkGroupID.x;\n";
    glsl += "    const uint base = tri * 3u;\n";
    glsl += "\n";
    glsl += "    const vec3 p0 = u_PositionData.positions[base + 0u];\n";
    glsl += "    const vec3 p1 = u_PositionData.positions[base + 1u];\n";
    glsl += "    const vec3 p2 = u_PositionData.positions[base + 2u];\n";
    glsl += "\n";
    glsl += "    gl_MeshVerticesEXT[0].gl_Position = ToClip(p0);\n";
    glsl += "    gl_MeshVerticesEXT[1].gl_Position = ToClip(p1);\n";
    glsl += "    gl_MeshVerticesEXT[2].gl_Position = ToClip(p2);\n";
    glsl += "\n";
    glsl += "    fragColor[0] = FetchColor(base + 0u);\n";
    glsl += "    fragColor[1] = FetchColor(base + 1u);\n";
    glsl += "    fragColor[2] = FetchColor(base + 2u);\n";
    glsl += "\n";
    glsl += "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n";
    glsl += "}\n";

    return glsl;
}

}

MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,
                                          const Material2DCreateConfig *cfg,
                                          const MaterialVariantDesc &desc,
                                          const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return(nullptr);

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, false);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    vertices.push_back({VAT_VEC4, VAN::Color});

    MaterialResourceManifest manifest;
    StaticMaterialDef def{};
    build2d::BuildBase2DFixedDef(def,
                                 "VertexColor2D",
                                 cfg,
                                 vertices,
                                 manifest);

    if (UsesMeshPipeline(*cfg))
    {
        const std::string mesh_glsl = BuildVertexColor2DMeshShader(cfg, key);
        if (mesh_glsl.empty())
        {
            std::fprintf(stderr,
                "[VertexColor2D] mesh path requires position_provider=SSBO_PackedVec3 and color attribute_provider in {SSBO_PackedRGBA8, SSBO_Vec4}\n");
            return nullptr;
        }

        CompositorAssembler assembler;
        const auto result = assembler.Assemble(key, desc);
        if (!result.success)
        {
            std::fprintf(stderr, "[VertexColor2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
            return nullptr;
        }

        def.vertex_stream_key = &key;
        const std::string fs = fs_preamble + result.fragment_glsl;

        MaterialCreateInfo *mci = CompileCompositorMaterial(profile,
                                                            def,
                                                            mesh_glsl,
                                                            fs,
                                                            cfg);
        if (!mci)
            std::fprintf(stderr, "[VertexColor2D] CompileCompositorMaterial(mesh) failed\n");

        return mci;
    }

    return CreateFromFixedDef2D("VertexColor2D", profile, def, key, vs_preamble, fs_preamble, cfg, desc);
}

static MaterialCreateInfo *VertexColor2D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateVertexColor2D(profile, static_cast<const Material2DCreateConfig *>(cfg), *desc, key); }

}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexColor2D, "VertexColor2D", hgl::graph::mtl::VertexColor2D_Adapter)
