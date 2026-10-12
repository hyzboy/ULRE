// VertexABIBuilder.cpp — 顶点 ABI 求解（s1_* 模块选择 + VkFormat→GLSL 类型映射）
//
// 从 MaterialDefinitionRegistry.cpp 拆分（5 职责之一：vertex_abi）。
// 公开声明仍在 MaterialDefinitionRegistry.h（调用者零改动）。

#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/ShaderCodeModuleRegistry.h>
#include<hgl/mtl/ShaderLibraryPath.h>
#include<hgl/log/Log.h>
#include<cstring>
#include<algorithm>
#include<vector>
#include<string>

namespace hgl::graph::mtl{

namespace
{
    bool BuildResolvedVertexABI(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        VkFormat &out_position_format,
        std::string &out_vertex_input_glsl,
        ShaderCodeModuleResolutionResult &out_resolution)
    {
        switch (definition.vertex_provider_policy)
        {
        case MaterialVertexProviderPolicy::Auto:
        case MaterialVertexProviderPolicy::GeometryOnly:
        case MaterialVertexProviderPolicy::AllowDerived:
            break;
        default:
            return false;
        }
        if (!request.geometry_vertex_format)
            return false;

        // SSBO 顶点输入：无模块能力匹配（能力解析系统已删）——顶点输入由
        // 下方 s1_* 模块选择完成，resolved 恒为 true。
        out_resolution = ShaderCodeModuleResolutionResult{};
        out_resolution.resolved = true;

        const GeometryVertexFormat &geometry = *request.geometry_vertex_format;
        out_vertex_input_glsl.clear();
        out_position_format = VK_FORMAT_UNDEFINED;

        // SSBO 顶点输入：C++ 只做"选择"——按需求语义选 s1_* 模块，
        // 读取代码在模块内（gl_VertexIndex），无 VBO attribute 布局。
        {
            const GeometryVertexAttributeFormat *position_attribute =
                geometry.Find(VertexSemantic::Position);
            if (!position_attribute)
                return false;
            out_position_format = position_attribute->format;

            bool need_uv = false, need_ntb = false, need_color = false, need_luminance = false, need_transform_id = false, need_size = false;
            for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
            {
                const auto &requirement = definition.vertex_semantic_requirements[i];
                const VertexSemantic semantic =
                    GetVertexSemanticFromShaderCodeModuleSemantic(requirement.semantic);
                switch (semantic)
                {
                case VertexSemantic::TexCoord: need_uv = true; break;
                case VertexSemantic::Normal:
                case VertexSemantic::Tangent:
                case VertexSemantic::Bitangent: need_ntb = true; break;
                case VertexSemantic::Color: need_color = true; break;
                case VertexSemantic::Luminance: need_luminance = true; break;
                case VertexSemantic::TransformID: need_transform_id = true; break;
                case VertexSemantic::Size: need_size = true; break;
                default: break;  // Position 由 input mode 决定
                }
            }

            // 各数据模块先 include（定义 HGL_*_LOADER 宏），
            // Position 模块最后（LoadVertexData 以 #ifdef 展开全部 loader）
            if (need_uv)
            {
                // 按 UV 属性格式选解码模块（格式=模块——RG16F 发行版一模块）
                const auto *uv_attr = geometry.Find(VertexSemantic::TexCoord);
                if (uv_attr && uv_attr->format == VK_FORMAT_R16G16_SFLOAT)
                    out_vertex_input_glsl += "#include \"vertex/s1_uv_rg16f.glsl\"\n";
                else
                    out_vertex_input_glsl += "#include \"vertex/s1_uv.glsl\"\n";
            }
            if (need_ntb)
            {
                // 按 Normal 属性格式选解码模块（格式=模块——发行版压缩格式各一模块）
                const auto *normal_attr = geometry.Find(VertexSemantic::Normal);
                if (normal_attr && normal_attr->format == VK_FORMAT_R8G8_UNORM)
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb_rg8.glsl\"\n";
                else if (normal_attr && normal_attr->format == VK_FORMAT_R16G16_SFLOAT)
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb_rg16f.glsl\"\n";
                else if (normal_attr && normal_attr->format == PF_A2BGR10UN)
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb_a2bgr10.glsl\"\n";
                else
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb.glsl\"\n";
            }
            if (need_color)
            {
                if (definition.vertex_varying.emit_vertex_color_from_palette)
                    out_vertex_input_glsl += "#include \"vertex/s1_palette_index.glsl\"\n";   // palette 材质：ColorIndex（R8_UINT 索引）
                else
                    out_vertex_input_glsl += "#include \"vertex/s1_color.glsl\"\n";           // 标准顶点色：vec4 直读
            }
            if (need_luminance)
                out_vertex_input_glsl += "#include \"vertex/s1_luminance.glsl\"\n";
            if (need_transform_id)
                out_vertex_input_glsl += "#include \"vertex/s1_transform_id.glsl\"\n";
            if (need_size)
                out_vertex_input_glsl += "#include \"vertex/s1_size.glsl\"\n";

            // 位置模块按 effective input 选择（position_format 判定——与
            // MeshTemplateEmitter 一致：geometry 格式说了算，recipe/TOML input 仅兜底）
            VertexInputMode effective_input = definition.vertex_node_config.input;
            if (out_position_format == VK_FORMAT_R32G32_SINT || out_position_format == VK_FORMAT_R32G32_UINT ||
                out_position_format == VK_FORMAT_R16G16_SINT || out_position_format == VK_FORMAT_R16G16_UINT)
                effective_input = VertexInputMode::Vec2IntPosition;
            else if (out_position_format == VK_FORMAT_R32G32_SFLOAT ||
                     out_position_format == VK_FORMAT_R16G16_SFLOAT)
                effective_input = VertexInputMode::Vec2Position;
            else if (out_position_format == VK_FORMAT_R32G32B32_SFLOAT ||
                     out_position_format == VK_FORMAT_R32G32B32A32_SFLOAT)
                effective_input = VertexInputMode::Vec3Position;

            switch (effective_input)
            {
            case VertexInputMode::Vec3Position:
                out_vertex_input_glsl += "#include \"vertex/s1_position_vec3.glsl\"\n";
                break;
            case VertexInputMode::Vec2IntPosition:
                out_vertex_input_glsl += "#include \"vertex/s1_position_vec2i.glsl\"\n";
                break;
            case VertexInputMode::Vec2Position:
                out_vertex_input_glsl += "#include \"vertex/s1_position_vec2.glsl\"\n";
                break;
            case VertexInputMode::None:
                break;   // 无外部顶点输入（CharQuad 等自持全部 SSBO，不 include 顶点输入模块）
            default:
                return false;
            }
        }

        return out_position_format != VK_FORMAT_UNDEFINED;
    }
}

VkFormat ResolveMaterialVertexSemanticFormat(const GeometryVertexFormat *gvf, VertexSemantic semantic, VkFormat fallback_format)
{
    if(!gvf)
        return fallback_format;

    const GeometryVertexAttributeFormat *attribute=gvf->Find(semantic);
    if(!attribute||attribute->format==VK_FORMAT_UNDEFINED)
        return fallback_format;

    return attribute->format;
}

bool BuildResolvedMaterialVertexABI(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialResolvedVertexABI &out_abi)
{
    std::string vertex_input_glsl;
    VkFormat position_format = VK_FORMAT_UNDEFINED;
    ShaderCodeModuleResolutionResult resolution;
    if (!BuildResolvedVertexABI(definition, request, position_format,
                                vertex_input_glsl, resolution))
        return false;

    out_abi.position_format = position_format;
    out_abi.provider_graph_hash =
        GetShaderCodeModuleProviderGraphHash(resolution);
    out_abi.vertex_input_glsl = vertex_input_glsl.c_str();
    if (!ComposeShaderCodeModuleProviderGraph(resolution, out_abi.provider_glsl))
        return false;
    return true;
}

}//namespace hgl::graph::mtl
