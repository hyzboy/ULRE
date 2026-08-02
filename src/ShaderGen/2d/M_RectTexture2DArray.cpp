#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredRectTexture2DArrayBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "RectTexture2DArray";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::RectTexture2DArray);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        bmi.with_local_to_world = true;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2DArray, true}};
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::EmissiveSurface}};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::RectTexture2DArray, bmi);
        return true;
    }();
}

static ShaderProgramBuildSpec *CreateRectTexture2DArrayImpl(const contract::PhysicalDeviceProfileLite *profile, Material2DBuildParams p)
{
    constexpr const char mi_codes[]="uvec4 id;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4u);
    if(!profile)
        return(nullptr);

    p.prim = PrimitiveType::Triangles;
    p.shader_stage_flag_bit &= ~(uint32_t)ShaderStage::Geometry;

    const VkFormat position_format = ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);
    auto preamble = build2d::Build2DPreamble(p, true, position_format);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p, position_format);
    build2d::PushSemanticVertexEntry(vertices, p, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);

    FixedMaterialDef def {
        "RectTexture2DArray",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/recttexture2darray.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/recttexture2darray.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[RectTexture2DArray] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateRectTexture2DArray(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateRectTexture2DArrayImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
