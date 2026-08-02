#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexColor2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexColor2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexColor2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexColor2D, bmi);
        return true;
    }();
}

static ShaderProgramBuildSpec *CreateVertexColor2DImpl(const contract::PhysicalDeviceProfileLite *profile, const Material2DBuildParams &p)
{
    auto preamble = build2d::Build2DFragmentPreamble(p, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p);
    build2d::PushSemanticVertexEntry(vertices, p, VertexSemantic::Color, VK_FORMAT_R32G32B32A32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);

    FixedMaterialDef def {
        "VertexColor2D",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    const VkFormat position_format = ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);
    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_vertex_color = true;
    const std::string extra_attrs = "layout(location=1) in vec4 Color;\n";
    std::string vs = GenerateVertexShader(
        p.vertex_node_config,
        varying_cfg,
        position_format,
        extra_attrs,
        "ShaderLibrary"
    );
    std::string fs = preamble + "#include \"2d/vertexcolor2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[VertexColor2D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateVertexColor2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
