#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<string>
#include "SharedDescriptors3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexLuminance3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexLuminance3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexLuminance3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = false;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexLuminance3D, bmi);
        return true;
    }();

    constexpr const char VERTEX_LUMINANCE_3D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_3D_MI_BYTES = sizeof(hgl::math::Vector4f);

}

static ShaderProgramBuildSpec *CreateVertexLuminance3DImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc)
{
    bc.material_instance = true;

    const VkFormat position_format = ResolveMaterialPositionFormat(bc.geometry_vertex_format, VK_FORMAT_R32G32B32_SFLOAT);
    const bool use_vec2_position = position_format == VK_FORMAT_R32G32_SFLOAT;
    const VkFormat luminance_format = ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Luminance, VK_FORMAT_R32_SFLOAT);

    FixedVertexEntry vertex_luminance_3d_vertex[] = {
        { position_format,   VertexSemantic::Position },
        { luminance_format,  VertexSemantic::Luminance },
    };

    FixedMaterialDef dynamic_def {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        vertex_luminance_3d_vertex,
        uint32_t(sizeof(vertex_luminance_3d_vertex) / sizeof(vertex_luminance_3d_vertex[0])),
        kBase3DWithMIDescriptors,
        kBase3DWithMIDescriptorCount,
        VERTEX_LUMINANCE_3D_MI_CODES,
        VERTEX_LUMINANCE_3D_MI_BYTES,
    };

    CompositorAssembler assembler("ShaderLibrary");

    const char *vs_template = use_vec2_position
        ? "compositor/main_forward_unlit_luminance_2d.vert.glsl"
        : "compositor/main_forward_unlit_luminance.vert.glsl";

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        vs_template,
        "compositor/main_forward_unlit_luminance.frag.glsl",
        "surface/unlit_luminance_surface.glsl"
    );

    if (!result.success)
    {
        GLogError("[VertexLuminance3D] CompositorAssembler failed: %s",
                  result.error_message.c_str());
        return nullptr;
    }

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        bc);

    if (!mci)
        GLogError("[VertexLuminance3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateVertexLuminance3DImpl(profile, ToCompositorBuildConfig3D(request, definition));
}
}//namespace hgl::graph::mtl
