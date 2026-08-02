#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl
{
namespace
{
    const bool kRegisteredGizmo3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "Gizmo3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::Gizmo3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = false;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::Gizmo3D, bmi);
        return true;
    }();

    // ─────────────────────────────────────────────────────────────────────────────
    // 材质实例定义
    // ─────────────────────────────────────────────────────────────────────────────

    constexpr const char GIZMO_3D_MI_GLSL[] = "vec4 Color;";
    constexpr uint32_t GIZMO_3D_MI_BYTES = sizeof(math::Vector4f);

}

static ShaderProgramBuildSpec *CreateGizmo3DImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc, const MaterialDefinition &definition)
{
    CompositorAssembler assembler("ShaderLibrary");

    auto fs_result = assembler.AssembleFragment(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_unlit_normal.frag.glsl",
        "surface/gizmo3d_surface.glsl"
    );

    if (!fs_result.success)
    {
        GLogError("[Gizmo3D] CompositorAssembler failed: %s",
                  fs_result.error_message.c_str());
        return nullptr;
    }

    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);

    const vertex_builder_common::VertexSemanticDecl vertex_decls[] = {
        { VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT },
        { VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT },
    };
    const vertex_builder_common::VertexBuildInput vertex_input {
        PrimitiveType::Triangles,
        vertex_builder_common::VertexTransformIntent::LocalToWorld,
        bc.geometry_vertex_format,
        vertex_decls,
        2
    };
    std::vector<FixedVertexEntry> gizmo_3d_vertex = vertex_builder_common::BuildVertexEntries(vertex_input);

    FixedMaterialDef dynamic_def {
        "Gizmo3D",
        PrimitiveType::Triangles,
        gizmo_3d_vertex.data(),
        uint32_t(gizmo_3d_vertex.size()),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
        GIZMO_3D_MI_GLSL,
        GIZMO_3D_MI_BYTES,
    };

    // emit_world_pos + emit_world_normal: matches main_forward_unlit_normal.frag.glsl interface
    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_data_index_id    = true;
    varying_cfg.emit_texture_layer_id = true;
    varying_cfg.texture_layer_id_uses_data_index = true;
    varying_cfg.emit_world_pos        = true;
    varying_cfg.emit_world_normal     = true;
    const std::string extra_attrs = "layout(location=1) in vec3 Normal;\n";
    std::string vs_glsl = GenerateVertexShader(
        definition.vertex_node_config,
        varying_cfg,
        VK_FORMAT_R32G32B32_SFLOAT,
        extra_attrs,
        "ShaderLibrary"
    );

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        vs_glsl,
        fs_result.fragment_glsl,
        bc);

    if (!mci)
        GLogError("[Gizmo3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateGizmo3DImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
