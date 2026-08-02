#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<string>
#include "SharedDescriptors3D.h"

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
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::Gizmo3D, bmi);
        return true;
    }();

    // ─────────────────────────────────────────────────────────────────────────────
    // 材质实例定义
    // ─────────────────────────────────────────────────────────────────────────────

    constexpr const char GIZMO_3D_MI_GLSL[] = "vec4 Color;";
    constexpr uint32_t GIZMO_3D_MI_BYTES = sizeof(math::Vector4f);

}

static ShaderProgramBuildSpec *CreateGizmo3DImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc)
{
    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_unlit_normal.vert.glsl",
        "compositor/main_forward_unlit_normal.frag.glsl",
        "surface/gizmo3d_surface.glsl"
    );

    if (!result.success)
    {
        GLogError("[Gizmo3D] CompositorAssembler failed: %s",
                  result.error_message.c_str());
        return nullptr;
    }

    bc.material_instance = true;

    FixedVertexEntry gizmo_3d_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Normal },
    };

    FixedMaterialDef dynamic_def {
        "Gizmo3D",
        PrimitiveType::Triangles,
        gizmo_3d_vertex,
        uint32_t(sizeof(gizmo_3d_vertex) / sizeof(gizmo_3d_vertex[0])),
        kBase3DWithMIDescriptors,
        kBase3DWithMIDescriptorCount,
        GIZMO_3D_MI_GLSL,
        GIZMO_3D_MI_BYTES,
    };

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        bc);

    if (!mci)
        GLogError("[Gizmo3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateGizmo3DImpl(profile, ToCompositorBuildConfig3D(request, definition));
}
}//namespace hgl::graph::mtl
