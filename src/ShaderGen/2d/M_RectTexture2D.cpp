#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredRectTexture2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "RectTexture2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::RectTexture2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true}};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::RectTexture2D, bmi);
        return true;
    }();
}

static ShaderProgramBuildSpec *CreateRectTexture2DImpl(const contract::PhysicalDeviceProfileLite *profile, Material2DBuildParams p)
{
    if(!profile)
        return(nullptr);

    p.prim = PrimitiveType::Triangles;
    p.shader_stage_flag_bit &= ~(uint32_t)ShaderStage::Geometry;

    const VkFormat position_format = ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);

    auto preamble = build2d::Build2DFragmentPreamble(p, true);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p, position_format);
    build2d::PushSemanticVertexEntry(vertices, p, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);

    FixedMaterialDef def {
        "RectTexture2D",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_uv0 = true;
    const std::string extra_attrs = "layout(location=1) in vec2 TexCoord;\n";
    std::string vs = GenerateVertexShader(
        p.vertex_node_config,
        varying_cfg,
        position_format,
        extra_attrs,
        "ShaderLibrary"
    );
    std::string fs = preamble + "#include \"2d/puretexture2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[RectTexture2D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateRectTexture2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateRectTexture2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
