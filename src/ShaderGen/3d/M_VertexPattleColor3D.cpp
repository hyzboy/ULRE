/** 顶点调色板色要求有一个UBO结构如下
*
*
*   struct ColorPattle
*   {
*       vec4 color[256];
*   }color_pattle;
*
*   然后输入的一个R8UI顶点属性来指定使用那个颜色。
*/

#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/graph/ShaderBufferSources.h>
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
    const bool kRegisteredVertexPattleColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexPattleColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexPattleColor3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo, UBODescriptorSemantic::MaterialColorPalette};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexPattleColor3D, bmi);
        return true;
    }();

}//namespace

static ShaderProgramBuildSpec *CreateVertexPattleColor3DImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc, const MaterialDefinition &definition)
{
    static const ShaderBufferSource * const private_sbs_list[] = { &SBS_ColorPattle };
    bc.private_shader_buffer_sources = private_sbs_list;
    bc.private_shader_buffer_source_count = 1;
    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);

    const vertex_builder_common::VertexSemanticDecl vertex_decls[] = {
        { VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT },
        { VertexSemantic::Color,    VK_FORMAT_R32_UINT },
    };
    const vertex_builder_common::VertexBuildInput vertex_input {
        PrimitiveType::Triangles,
        bc.geometry_vertex_format,
        vertex_decls,
        2
    };
    std::vector<FixedVertexEntry> vertex_pattle_color_3d_vertex = vertex_builder_common::BuildVertexEntries(vertex_input);
    vertex_builder_common::AppendTransformIDVertexEntry(vertex_pattle_color_3d_vertex);

    FixedMaterialDef dynamic_def {
        "VertexPattleColor3D",
        PrimitiveType::Triangles,
        vertex_pattle_color_3d_vertex.data(),
        uint32_t(vertex_pattle_color_3d_vertex.size()),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
    };

    CompositorAssembler assembler("ShaderLibrary");
    auto fs_result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        "compositor/main_forward_unlit_vertexcolor.frag.glsl",
        "surface/unlit_vertexcolor_surface.glsl"
    );

    if (!fs_result.success)
    {
        GLogError("[VertexPattleColor3D] CompositorAssembler failed: %s",
                  fs_result.error_message.c_str());
        return nullptr;
    }

    // emit_vertex_color(from_pattle) + use_transform_id_attr:
    // TransformID attribute indexes l2w.mats directly (no instance table);
    // ColorIndex attribute samples the MaterialColorPalette UBO.
    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_vertex_color_from_pattle = true;
    varying_cfg.use_transform_id_attr         = true;

    const std::string extra_attrs =
        "layout(location=1) in uint ColorIndex;\n"
        "layout(location=2) in uint TransformID;\n";
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
        GLogError("[VertexPattleColor3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateVertexPattleColor3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateVertexPattleColor3DImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
