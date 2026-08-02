#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureColor3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.usage_tag   = MaterialDefinitionUsageTag::Fallback;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureColor3D, bmi);

        // builtin/fallback_3d: 3D 无材质保底
        {
            MaterialDefinition alias = bmi;
            alias.definition_id = BUILTIN_MTL_DEF_FALLBACK_3D;
            alias.definition_name = "builtin/fallback_3d";
            RegisterMaterialDefinition(alias);
        }

        // builtin/missing_material: 缺失材质（当前与 fallback_3d 相同，未来可换成棋盘格）
        {
            MaterialDefinition alias = bmi;
            alias.definition_id = BUILTIN_MTL_DEF_MISSING_MATERIAL;
            alias.definition_name = "builtin/missing_material";
            RegisterMaterialDefinition(alias);
        }

        return true;
    }();

}

static ShaderProgramBuildSpec *CreatePureColor3DImpl(const contract::PhysicalDeviceProfileLite *profile, const CompositorMaterialBuildConfig &bc, const MaterialDefinition &definition)
{
    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);

    const vertex_builder_common::VertexSemanticDecl vertex_decls[] = {
        { VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT },
    };
    const vertex_builder_common::VertexBuildInput vertex_input {
        PrimitiveType::Triangles,
        bc.geometry_vertex_format,
        vertex_decls,
        1
    };
    std::vector<FixedVertexEntry> pure_color_3d_vertex = vertex_builder_common::BuildVertexEntries(vertex_input);

    FixedMaterialDef dynamic_def {
        "PureColor3D",
        PrimitiveType::Triangles,
        pure_color_3d_vertex.data(),
        uint32_t(pure_color_3d_vertex.size()),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto fs_result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque
    );

    if (!fs_result.success)
    {
        GLogError("[PureColor3D] CompositorAssembler failed: %s",
                  fs_result.error_message.c_str());
        return nullptr;
    }

    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_data_index_id = true;
    varying_cfg.emit_texture_layer_id = true;
    varying_cfg.texture_layer_id_uses_data_index = true;
    std::string vs_glsl = GenerateVertexShader(
        definition.vertex_node_config,
        varying_cfg,
        VK_FORMAT_R32G32B32_SFLOAT,
        "",
        "ShaderLibrary"
    );

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        vs_glsl,
        fs_result.fragment_glsl,
        bc);

    if (!mci)
        GLogError("[PureColor3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    CompositorMaterialBuildConfig bc = ToCompositorBuildConfig3D(request, definition);
    return CreatePureColor3DImpl(profile, bc, definition);
}
}//namespace hgl::graph::mtl
