#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredText2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "Text2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::Text2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.usage_tag = MaterialDefinitionUsageTag::Text;
        bmi.is_text = true;          // text 材质语义标记
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::Ortho;
        bmi.local_to_world_2d = false;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true, SamplerName::Text}};
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::TransmissionSurface}};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::Text2D, bmi);

        MaterialDefinition alias = bmi;
        alias.definition_id = BUILTIN_MTL_DEF_TEXT;
        alias.definition_name = "builtin/text";
        RegisterMaterialDefinition(alias);

        return true;
    }();
}

static ShaderProgramBuildSpec *CreateText2DImpl(const contract::PhysicalDeviceProfileLite *profile, Material2DBuildParams p)
{
    constexpr const char mi_codes[]="uint TextColor;";
    constexpr const uint32_t mi_bytes=sizeof(uint32);
    if(!profile)
        return(nullptr);

    p.prim = PrimitiveType::Triangles;
    p.shader_stage_flag_bit &= ~(uint32_t)ShaderStage::Geometry;

    const VkFormat position_format = ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SINT);
    auto preamble = build2d::Build2DPreamble(p, true, position_format);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p, position_format);
    build2d::PushSemanticVertexEntry(vertices, p, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);

    FixedMaterialDef def {
        "Text2D",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/text2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/text2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[Text2D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateText2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
