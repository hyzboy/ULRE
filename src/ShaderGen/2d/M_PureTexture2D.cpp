#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureTexture2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureTexture2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureTexture2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        bmi.with_local_to_world = true;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true}};
        bmi.vertex_node_config = MakeNodeConfigFrom2D(bmi.coordinate_system_2d, bmi.local_to_world_2d);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureTexture2D, bmi);
        return true;
    }();
}

static ShaderProgramBuildSpec *CreatePureTexture2DImpl(const contract::PhysicalDeviceProfileLite *profile, const Material2DBuildParams &p)
{
    auto preamble = build2d::Build2DPreamble(p, true);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p);
    build2d::PushSemanticVertexEntry(vertices, p, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);

    FixedMaterialDef def {
        "PureTexture2D",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    const VkFormat position_format = ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);
    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_uv0 = true;
    const std::string extra_attrs = "layout(location=1) in vec2 TexCoord;\n";
    std::string vs = GenerateVertexShader(
        MakeNodeConfigFrom2D(p.coordinate_system, p.local_to_world),
        varying_cfg,
        position_format,
        extra_attrs,
        "ShaderLibrary"
    );
    std::string fs = preamble + "#include \"2d/puretexture2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[PureTexture2D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreatePureTexture2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
