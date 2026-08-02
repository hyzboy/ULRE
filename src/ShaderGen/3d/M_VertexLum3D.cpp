#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexLuminance3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexLuminance3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexLuminance3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexLuminance3D, bmi);
        return true;
    }();

    constexpr const char VERTEX_LUMINANCE_3D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_3D_MI_BYTES = sizeof(hgl::math::Vector4f);

    static std::string BuildVertexLuminance3DVertexShader(const bool use_vec2_position)
    {
        std::string vs =
            "#version 450\n\n"
            "#include \"common/descriptor_macros.glsl\"\n"
            "#include \"common/scene_ubo.glsl\"\n"
            "SCENE_CAMERA_UBO;\n"
            "#include \"common/l2w_ssbo.glsl\"\n"
            "L2W_SSBO;\n"
            "#include \"common/instance_rows_ssbo.glsl\"\n"
            "L2W_INDEX_ROWS_SSBO;\n"
            "DATA_INDEX_ROWS_SSBO;\n\n";

        vs += use_vec2_position
            ? "#include \"vertex/s1_input_vec2.glsl\"\n"
            : "#include \"vertex/s1_input_vec3.glsl\"\n";

        vs += "layout(location=1) in float Luminance;\n";
        vs += use_vec2_position
            ? "#include \"vertex/s2_lift_xy0.glsl\"\n"
            : "#include \"vertex/s2_passthrough3d.glsl\"\n";
        vs += "#include \"vertex/helpers/orient_world.glsl\"\n\n";
        vs += "layout(location=0) flat out uint fragDataIndexID;\n";
        vs += "layout(location=1) flat out uint fragTextureLayerID;\n";
        vs += "layout(location=2) out float fragLuminance;\n\n";
        vs += "void main()\n";
        vs += "{\n";
        vs += "    fragDataIndexID = ResolveDataIndexID(gl_InstanceIndex);\n";
        vs += "    fragTextureLayerID = fragDataIndexID;\n";
        vs += "    fragLuminance = Luminance;\n";
        vs += "    vec4 world_pos = GetL2W() * GetLocalPos();\n";
        vs += "    gl_Position = camera.vp * world_pos;\n";
        vs += "}\n";
        return vs;
    }

}

static ShaderProgramBuildSpec *CreateVertexLuminance3DImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc, const MaterialDefinition &definition)
{
    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);
    const vertex_builder_common::LuminanceVertexBuildResult vertex_result =
        vertex_builder_common::BuildLuminanceVertexEntries(bc.geometry_vertex_format);

    FixedMaterialDef dynamic_def {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        vertex_result.entries.data(),
        uint32_t(vertex_result.entries.size()),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
        VERTEX_LUMINANCE_3D_MI_CODES,
        VERTEX_LUMINANCE_3D_MI_BYTES,
    };

    CompositorAssembler assembler("ShaderLibrary");
    auto fs_result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_unlit_luminance.frag.glsl",
        "surface/unlit_luminance_surface.glsl"
    );

    if (!fs_result.success)
    {
        GLogError("[VertexLuminance3D] CompositorAssembler failed: %s",
                  fs_result.error_message.c_str());
        return nullptr;
    }

    const std::string vs_glsl = BuildVertexLuminance3DVertexShader(vertex_result.use_vec2_position);

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        vs_glsl,
        fs_result.fragment_glsl,
        bc);

    if (!mci)
        GLogError("[VertexLuminance3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateVertexLuminance3DImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
