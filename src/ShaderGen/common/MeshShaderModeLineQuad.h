// MeshShaderModeLineQuad.h — LineQuad 模式 main() 体
//
// 每线程 1 线段 → 4 顶点 2 三角形（line-to-quad 展开）。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <vulkan/vulkan.h>
#include <string>
#include "VertexVaryingConfig.h"
#include "MeshShaderModeVertexPassthrough.h"   // MeshShaderModeContext

namespace hgl::graph::mtl
{
    inline void EmitLineQuadBody(
        std::string &ms,
        const MeshShaderModeContext &ctx,
        VkFormat position_format)
    {
        const auto &resolved_stage_interface = *ctx.stage_interface;
        const auto &varying_cfg = *ctx.varying_cfg;

        // 每线程 1 线段 → 4 顶点 2 三角形（line-to-quad）
        const std::string group_size = std::to_string(ctx.max_invocations);

        // 全局线段索引 = threadgroup 索引 × group size + 局部索引
        // （vkCmdDrawMeshTasksEXT 派发多个 threadgroup，每个处理一段连续线段）
        ms += "    const uint line_id = gl_WorkGroupID.x * ";
        ms += group_size;
        ms += "u + gl_LocalInvocationIndex;\n";
        // threadgroup 内顶点槽位基址（0 .. group_size*4-1，即 0..max_vertices-1）
        ms += "    const uint vid = gl_LocalInvocationIndex * 4u;\n";
        ms += "\n";
        ms += "    const uint total_lines = pc_vertex_index.total_vertices >> 1u;\n";
        // 本 threadgroup 有效线段数（所有 invocation 计算相同值 → SetMeshOutputsEXT 一致；
        // groupCountX = ceil(total_lines / group_size)，末组起始 <= total_lines，不会 uint 下溢）
        ms += "    const uint lines_this_group = min(";
        ms += group_size;
        ms += "u, total_lines - gl_WorkGroupID.x * ";
        ms += group_size;
        ms += "u);\n";
        ms += "    SetMeshOutputsEXT(lines_this_group * 4u, lines_this_group * 2u);\n";
        ms += "\n";
        // 越界 invocation 不输出（只读 SSBO 不写顶点）
        ms += "    if (line_id >= total_lines)\n";
        ms += "        return;\n";
        ms += "\n";

        // 线段端点（每线段 2 顶点——直接读 SSBO，不依赖 LoadVertexData 的全局变量：
        // LineQuad 每线程处理 1 条线段（2 顶点），LoadVertexData 只读 1 顶点，
        // 其 Width/TransformID/ColorIndex 全局变量在 LineQuad 下从不赋值 → NaN）
        // 支持索引/非索引：非索引顶点号 = vertex_base + 绘制序号（每 2 连续 1 线段）；
        // 索引走 sbo_vertex_index 查表（线段 = 每 2 连续索引，索引值 + vertex_base 定位——
        // 与 VS 的 s1_index 语义一致；BoundingBox 线框即 8 顶点 + 24 索引的索引几何）
        ms += "    uint v0 = pc_vertex_index.vertex_base + line_id * 2u;\n";
        ms += "    uint v1 = v0 + 1u;\n";
        ms += "    if (pc_vertex_index.is_indexed != 0u)\n";
        ms += "    {\n";
        ms += "        const uint i0 = pc_vertex_index.index_base + line_id * 2u;\n";
        ms += "        v0 = pc_vertex_index.vertex_base + sbo_vertex_index.data[i0];\n";
        ms += "        v1 = pc_vertex_index.vertex_base + sbo_vertex_index.data[i0 + 1u];\n";
        ms += "    }\n";
        // 端点按 position_format 适配：vec2（2D 平面材质）→ vec3 构造；vec3 直读
        if (position_format == VK_FORMAT_R32G32_SFLOAT)
        {
            ms += "    const vec3 from = vec3(sbo_vertex_position.data[v0], 0.0);\n";
            ms += "    const vec3 to   = vec3(sbo_vertex_position.data[v1], 0.0);\n";
        }
        else
        {
            ms += "    const vec3 from = sbo_vertex_position.data[v0];\n";
            ms += "    const vec3 to   = sbo_vertex_position.data[v1];\n";
        }

        // 材质自适应：按实际 include 的 s1_* 模块（材质 requirements）选择读取。
        // LineQuad 不假设 palette/Size/TransformID 属性都存在——BBox 线等材质
        // 可能只有 Position（pure_color fallback 等），缺失的属性用 fallback：
        //   palette 颜色（S1_PALETTE_INDEX_GLSL）→ sbo_vertex_color R8 解码
        //   TransformID（S1_TRANSFORM_ID_GLSL）→ 直读 + l2w.mats[transform_id]；
        //     否则 Standard 路径 l2w_index 查表（ResolveTransformID(gl_InstanceIndex)）
        //   Size/宽度（S1_SIZE_GLSL）→ sbo_vertex_size（width/min_width 在下方统一线宽段定义）
        ms += "#ifdef S1_PALETTE_INDEX_GLSL\n";
        ms += "    const uint color_index = (sbo_vertex_color.data[v0 >> 2u] >> ((v0 & 3u) * 8u)) & 0xFFu;\n";
        ms += "#endif\n";
        ms += "#ifdef S1_TRANSFORM_ID_GLSL\n";
        ms += "    const uint transform_id = sbo_vertex_transform_id.data[v0];\n";
        ms += "    const mat4 l2w_m = l2w.mats[transform_id];\n";
        ms += "#else\n";
        ms += "    const mat4 l2w_m = l2w.mats[ResolveTransformID(gl_InstanceIndex)];\n";
        ms += "#endif\n";

        // 世界空间 quad 展开（l2w_m 已在材质自适应段定义）
        ms += "    const vec3 from_world = (l2w_m * vec4(from, 1.0)).xyz;\n";
        ms += "    const vec3 to_world   = (l2w_m * vec4(to, 1.0)).xyz;\n";

        // 屏幕空间线宽：width 为像素（与旧 vkCmdSetLineWidth 语义一致）。
        // clip 空间偏移——NDC 1 单位 = viewport 高度/2 像素（视口高从场景级
        // viewport UBO 读取：切换 FBO 必绑，所有材质无条件 include）：
        //   偏移_ndc = n_ndc * (width_pixels * 0.5) / (viewport_height * 0.5) = n_ndc * width_pixels / viewport_height
        ms += "    const vec4 c_from = camera.vp * vec4(from_world, 1.0);\n";
        ms += "    const vec4 c_to   = camera.vp * vec4(to_world, 1.0);\n";
        ms += "    const vec2 ndc_from = c_from.xy / c_from.w;\n";
        ms += "    const vec2 ndc_to   = c_to.xy / c_to.w;\n";
        ms += "    vec2 dir_ndc = ndc_to - ndc_from;\n";
        ms += "    if (length(dir_ndc) < 1e-6)\n";
        ms += "        dir_ndc = vec2(1.0, 0.0);   // 线段投影为点（朝向相机）——退化为水平方向\n";
        ms += "    dir_ndc = normalize(dir_ndc);\n";
        ms += "    const vec2 n_ndc = vec2(-dir_ndc.y, dir_ndc.x);\n";
        // 统一线宽（一套逻辑）：Size 语义 V2F 存 [满宽(最粗), 最细阈值]——
        //   width_eff = clamp(满宽 × 深度衰减, 最细, 满宽)
        //   有 Size（LineRenderPipeline 线宽入 SSBO）：.x=满宽 .y=最细（每线段可指定）
        //   无 Size（BBox 线等固定线框）：width=1 min=1 → 恒 1 像素（同一公式退化）
        ms += "#ifdef S1_SIZE_GLSL\n";
        ms += "    const vec2 line_size = sbo_vertex_size.data[v0];\n";
        ms += "    const float width = line_size.x;      // 满宽（最粗，用户指定）\n";
        ms += "    const float min_width = line_size.y;  // 最细阈值（用户指定；深度越大越细，clamp 到此下限）\n";
        ms += "#else\n";
        ms += "    const float width = 1.0;              // 无 Size（固定线框）：恒 1 像素\n";
        ms += "    const float min_width = 1.0;\n";
        ms += "#endif\n";
        ms += "    const float depth = 0.5 * (c_from.w + c_to.w);\n";
        ms += "    const float width_eff = clamp(width * (10.0 / max(depth, 1e-4)), min_width, width);\n";
        ms += "    const vec2 offset_ndc = n_ndc * (width_eff / float(viewport.viewport_resolution.y));\n";
        ms += "    vec4 c0 = c_from; c0.xy += offset_ndc * c_from.w;\n";
        ms += "    vec4 c1 = c_from; c1.xy -= offset_ndc * c_from.w;\n";
        ms += "    vec4 c2 = c_to;   c2.xy += offset_ndc * c_to.w;\n";
        ms += "    vec4 c3 = c_to;   c3.xy -= offset_ndc * c_to.w;\n";
        ms += "    gl_MeshVerticesEXT[vid + 0u].gl_Position = c0;\n";
        ms += "    gl_MeshVerticesEXT[vid + 1u].gl_Position = c1;\n";
        ms += "    gl_MeshVerticesEXT[vid + 2u].gl_Position = c2;\n";
        ms += "    gl_MeshVerticesEXT[vid + 3u].gl_Position = c3;\n";
        ms += "    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 0u] = uvec3(vid + 0u, vid + 1u, vid + 2u);\n";
        ms += "    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 1u] = uvec3(vid + 1u, vid + 3u, vid + 2u);\n";

        // varying（per-vertex；per-primitive 语义按图元号——每线段 2 图元共享）
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::DataIndexID))
        {
            // 与 VS 一致：实例 → MaterialPrivateDataIndexRows 查表（材质数据槽）
            ms += "    const uint data_id = ResolveMaterialPrivateDataIndex(gl_InstanceIndex);\n";
            ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 0u] = data_id;\n";
            ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 1u] = data_id;\n";
        }
        if (varying_cfg.emit_vertex_color_from_palette)
        {
            ms += "    const vec4 lcolor = unpackUnorm4x8(color_palette.color[color_index]);\n";
            ms += "    fragVertexColor[vid + 0u] = lcolor;\n";
            ms += "    fragVertexColor[vid + 1u] = lcolor;\n";
            ms += "    fragVertexColor[vid + 2u] = lcolor;\n";
            ms += "    fragVertexColor[vid + 3u] = lcolor;\n";
        }
        else if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::Color))
        {
            // 非 palette 顶点色（VertexColor 材质）：s1_color vec4 直读（LineQuad 不调
            // LoadVertexData——Color 全局变量未赋值，必须直读 SSBO）
            ms += "#ifdef S1_COLOR_GLSL\n";
            ms += "    const vec4 vcolor = sbo_vertex_color.data[v0];\n";
            ms += "    fragVertexColor[vid + 0u] = vcolor;\n";
            ms += "    fragVertexColor[vid + 1u] = vcolor;\n";
            ms += "    fragVertexColor[vid + 2u] = vcolor;\n";
            ms += "    fragVertexColor[vid + 3u] = vcolor;\n";
            ms += "#endif\n";
        }
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::Luminance))
        {
            // Luminance（VertexLuminance 材质）：R8 打包直读（与 s1_luminance 同解码公式）
            ms += "#ifdef S1_LUMINANCE_GLSL\n";
            ms += "    const uint lpacked = sbo_vertex_luminance.data[v0 >> 2u];\n";
            ms += "    const float lum = float((lpacked >> ((v0 & 3u) * 8u)) & 0xFFu) / 255.0;\n";
            ms += "    fragLuminance[vid + 0u] = lum;\n";
            ms += "    fragLuminance[vid + 1u] = lum;\n";
            ms += "    fragLuminance[vid + 2u] = lum;\n";
            ms += "    fragLuminance[vid + 3u] = lum;\n";
            ms += "#endif\n";
        }
    }
}
