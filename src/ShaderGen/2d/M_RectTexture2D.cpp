#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredRectTexture2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "RectTexture2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::RectTexture2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
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

    auto preamble = build2d::Build2DPreamble(p, true, false, position_format);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p, position_format);
    build2d::PushSemanticVertexEntry(vertices, p, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);
    descriptor_builder_common::PushMaterialSampler(
        descriptors,
        SamplerName::BaseColor,
        TextureSlot::BaseColor,
        "sampler2D",
        uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));

    FixedMaterialDef def {
        "RectTexture2D",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    std::string vs = preamble + "#include \"2d/puretexture2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/puretexture2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[RectTexture2D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateRectTexture2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    (void)definition;
    return CreateRectTexture2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
