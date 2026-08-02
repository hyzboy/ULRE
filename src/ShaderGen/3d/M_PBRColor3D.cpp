#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/log/Log.h>
#include <vector>

#include "../common/MFSkyLight.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPBRColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PBRColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PBRColor3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = true;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::PBRSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo, UBODescriptorSemantic::SkyInfo};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PBRColor3D, bmi);
        return true;
    }();

    constexpr const char mi_codes[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) + sizeof(float) * 2;

}//namespace

static ShaderProgramBuildSpec *CreatePBRColor3DImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc, const MaterialDefinition &definition)
{
    SkyLightAmbientModel ambient = bc.sky_ambient_model;

    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedVertexEntry pbr_color_3d_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT),    VertexSemantic::TexCoord },
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Normal },
    };

    FixedMaterialDef dynamic_def {
        "PBRColor3D",
        PrimitiveType::Triangles,
        pbr_color_3d_vertex,
        uint32_t(sizeof(pbr_color_3d_vertex) / sizeof(pbr_color_3d_vertex[0])),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
        mi_codes,
        mi_bytes,
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Standard,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_lit.vert.glsl",
        "compositor/main_forward_lit.frag.glsl",
        "surface/pbrcolor3d_surface.glsl"
    );

    if (!result.success)
    {
        GLogError("[PBRColor3D] CompositorAssembler failed: %s",
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
        GLogError("[PBRColor3D] CompileCompositorMaterial failed");

    return mci;
}

ShaderProgramBuildSpec *CreatePBRColor3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreatePBRColor3DImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
