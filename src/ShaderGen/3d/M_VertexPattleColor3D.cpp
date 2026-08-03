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
        const MaterialVertexAttributeDefinition attrs[] = {
            {VertexSemantic::Color, 1, VK_FORMAT_R32_UINT, "layout(location=1) in uint ColorIndex;\n"},
            {VertexSemantic::TransformID, 2, VK_FORMAT_R32_UINT, "layout(location=2) in uint TransformID;\n"}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_vertex_color_from_pattle = true;
        varying.use_transform_id_attr = true;
        ConfigureMaterialDefinitionContract(bmi, "compositor/main_forward_unlit_vertexcolor.frag.glsl",
                                             "surface/unlit_vertexcolor_surface.glsl",
                                             VK_FORMAT_R32G32B32_SFLOAT, varying, attrs, 2);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexPattleColor3D, bmi);
        return true;
    }();

}//namespace

void ForceLinkVertexPattleColor3DMaterialDefinition() {}

}//namespace hgl::graph::mtl