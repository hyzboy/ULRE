// MeshShaderModeCharQuad.h — CharQuad 模式 SSBO 声明 + main() 体
//
// 每线程 1 字符实例 → 6 顶点 2 三角形（字符 quad）。
// 三层数据模型：CharInfo + CharStyle + CharInstance。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <string>
#include "MeshShaderModeVertexPassthrough.h"   // MeshShaderModeContext

namespace hgl::graph::mtl
{
    // CharQuad SSBO 声明（全局作用域，void main 之前）
    inline void EmitCharQuadSSBODeclarations(std::string &ms)
    {
        ms += "// ── Text CharQuad SSBOs ──\n";
        ms += "struct TextCharInfo {\n";
        ms += "    int   offset_xy;\n";
        ms += "    uint  metrics_wh;\n";
        ms += "    uint  uv_lt;\n";
        ms += "    uint  uv_rb;\n";
        ms += "};\n";
        ms += "layout(set=PER_OBJECT_SET, binding=TEXT_CHARINFO_BINDING, std430) readonly buffer TextCharInfoData {\n";
        ms += "    TextCharInfo chars[];\n";
        ms += "} sbo_char_info;\n";
        ms += "\n";
        ms += "struct CharStyleData {\n";
        ms += "    uint  text_color;\n";
        ms += "    uint  outline_color;\n";
        ms += "    uint  shadow_color;\n";
        ms += "    uint  flags;\n";
        ms += "    float italic;\n";
        ms += "    float bold_px;\n";
        ms += "    float outline_px;\n";
        ms += "    uint  shadow_uv_offset;\n";
        ms += "    float scale;\n";
        ms += "    int   rotation;\n";
        ms += "};\n";
        ms += "layout(set=PER_OBJECT_SET, binding=TEXT_CHARSTYLE_BINDING, std430) readonly buffer CharStyleDataBuf {\n";
        ms += "    CharStyleData styles[];\n";
        ms += "} sbo_char_style;\n";
        ms += "\n";
        ms += "struct CharInstanceData {\n";
        ms += "    int   pen_xy;\n";
        ms += "    uint  char_style;\n";
        ms += "};\n";
        ms += "layout(set=PER_OBJECT_SET, binding=TEXT_CHARINSTANCE_BINDING, std430) readonly buffer CharInstanceDataBuf {\n";
        ms += "    CharInstanceData instances[];\n";
        ms += "} sbo_char_instance;\n";
        ms += "\n";
    }

    // CharQuad main() 体
    inline void EmitCharQuadBody(
        std::string &ms,
        const MeshShaderModeContext &ctx)
    {
        const auto &resolved_stage_interface = *ctx.stage_interface;

        // 每线程 1 字符实例 → 6 顶点 2 三角形（字符 quad）
        // 三层数据模型：CharInfo + CharStyle + CharInstance
        const std::string group_size = std::to_string(ctx.max_invocations);

        // ── 全局字符索引 + SetMeshOutputsEXT ──────────────────────
        ms += "    const uint char_idx = gl_WorkGroupID.x * ";
        ms += group_size;
        ms += "u + gl_LocalInvocationIndex;\n";
        ms += "    const uint base_vid = gl_LocalInvocationIndex * 6u;\n";
        ms += "\n";
        ms += "    const uint total_chars = pc_vertex_index.total_vertices;\n";
        ms += "    const uint chars_this_group = min(";
        ms += group_size;
        ms += "u, total_chars - gl_WorkGroupID.x * ";
        ms += group_size;
        ms += "u);\n";
        ms += "    SetMeshOutputsEXT(chars_this_group * 6u, chars_this_group * 2u);\n";
        ms += "\n";
        ms += "    if (char_idx >= total_chars)\n";
        ms += "        return;\n";
        ms += "\n";

        // ── 读取 CharInstance ─────────────────────────────────────
        ms += "    const CharInstanceData inst = sbo_char_instance.instances[char_idx];\n";
        ms += "    const int   pen_x    = (inst.pen_xy << 16) >> 16;\n";
        ms += "    const int   pen_y    = inst.pen_xy >> 16;\n";
        ms += "    const uint  char_id  = inst.char_style & 0xFFFFu;\n";
        ms += "    const uint  style_id = (inst.char_style >> 16) & 0xFFFFu;\n";
        ms += "\n";

        // ── 读取 TextCharInfo ─────────────────────────────────────
        ms += "    const TextCharInfo ci = sbo_char_info.chars[char_id];\n";
        ms += "    const int   mx = (ci.offset_xy << 16) >> 16;\n";
        ms += "    const int   my = ci.offset_xy >> 16;\n";
        ms += "    const uint  mw = ci.metrics_wh & 0xFFFFu;\n";
        ms += "    const uint  mh = (ci.metrics_wh >> 16) & 0xFFFFu;\n";
        ms += "\n";

        // ── 读取 CharStyleData ────────────────────────────────────
        ms += "    const CharStyleData cs = sbo_char_style.styles[style_id];\n";
        ms += "\n";

        // ── 计算 quad 像素坐标 ────────────────────────────────────
        // rect_top = pen_y - metrics_y + char_height
        // char_height 通过 MeshDrawParams.viewport_height 传递（CharQuad 模式复用）
        ms += "    const float char_scale = cs.scale;\n";
        ms += "    const int char_height = int(pc_vertex_index.viewport_height);\n";
        ms += "    const int mx_s = int(float(mx) * char_scale);\n";
        ms += "    const int my_s = int(float(my) * char_scale);\n";
        ms += "    const uint mw_s = uint(float(mw) * char_scale);\n";
        ms += "    const uint mh_s = uint(float(mh) * char_scale);\n";
        ms += "\n";

        // ── 旋转：计算旋转后的 quad 顶点坐标 ────────────────────────
        // 旋转中心 = 未加斜体的原始 quad 中心（旋转后斜体叠加在旋转结果上）
        // 90° 右转: (dx,dy)→(dy,-dx)  180°: (dx,dy)→(-dx,-dy)  270° 左转: (dx,dy)→(-dy,dx)
        ms += "    // Rotation: compute center, rotate offsets, add pen + shear\n";
        ms += "    const float half_mw = float(mw_s) * 0.5;\n";
        ms += "    const float half_mh = float(mh_s) * 0.5;\n";
        ms += "    const float cx = float(mx_s) + half_mw;\n";
        ms += "    const float cy = float(-my_s) + float(char_height) + half_mh;\n";
        ms += "    const int char_rot = cs.rotation;\n";
        ms += "    float r00 = 1.0, r01 = 0.0, r10 = 0.0, r11 = 1.0;\n";
        ms += "    if (char_rot == 90)       { r00 =  0.0; r01 =  1.0; r10 = -1.0; r11 =  0.0; }\n";
        ms += "    else if (char_rot == 180)  { r00 = -1.0; r01 =  0.0; r10 =  0.0; r11 = -1.0; }\n";
        ms += "    else if (char_rot == 270)  { r00 =  0.0; r01 = -1.0; r10 =  1.0; r11 =  0.0; }\n";
        ms += "    // TL offset from center\n";
        ms += "    float tl_dx = r00 * (-half_mw) - r01 * (-half_mh);\n";
        ms += "    float tl_dy = r10 * (-half_mw) + r11 * (-half_mh);\n";
        ms += "    // TR offset from center\n";
        ms += "    float tr_dx = r00 * (half_mw) - r01 * (-half_mh);\n";
        ms += "    float tr_dy = r10 * (half_mw) + r11 * (-half_mh);\n";
        ms += "    // BL offset from center\n";
        ms += "    float bl_dx = r00 * (-half_mw) - r01 * (half_mh);\n";
        ms += "    float bl_dy = r10 * (-half_mw) + r11 * (half_mh);\n";
        ms += "    // BR offset from center\n";
        ms += "    float br_dx = r00 * (half_mw) - r01 * (half_mh);\n";
        ms += "    float br_dy = r10 * (half_mw) + r11 * (half_mh);\n";
        ms += "    // Italic shear (relative to quad top, applied in local space before rotation)\n";
        ms += "    const float shear = tan(cs.italic);\n";
        ms += "    // Final positions: pen + rotated offset + shear offset\n";
        ms += "    const float pos_tl_x = float(pen_x) + cx + tl_dx + shear * (-half_mh - tl_dy);\n";
        ms += "    const float pos_tl_y = float(pen_y) + cy + tl_dy;\n";
        ms += "    const float pos_tr_x = float(pen_x) + cx + tr_dx + shear * (-half_mh - tr_dy);\n";
        ms += "    const float pos_tr_y = float(pen_y) + cy + tr_dy;\n";
        ms += "    const float pos_bl_x = float(pen_x) + cx + bl_dx + shear * (-half_mh - bl_dy);\n";
        ms += "    const float pos_bl_y = float(pen_y) + cy + bl_dy;\n";
        ms += "    const float pos_br_x = float(pen_x) + cx + br_dx + shear * (-half_mh - br_dy);\n";
        ms += "    const float pos_br_y = float(pen_y) + cy + br_dy;\n";
        ms += "\n";

        // ── UV 解包（half-float → float）─────────────────────────
        ms += "    const vec2 uv_lt = unpackHalf2x16(ci.uv_lt);  // .x=left, .y=top\n";
        ms += "    const vec2 uv_rb = unpackHalf2x16(ci.uv_rb);  // .x=right, .y=bottom\n";
        ms += "    const float uv_l = uv_lt.x;\n";
        ms += "    const float uv_t = uv_lt.y;\n";
        ms += "    const float uv_r = uv_rb.x;\n";
        ms += "    const float uv_b = uv_rb.y;\n";
        ms += "\n";

        // ── UV 旋转 ─────────────────────────────────────────────
        // 90°: (u,v)→(1-v,u)  180°: (u,v)→(1-u,1-v)  270°: (u,v)→(v,1-u)
        ms += "    // UV rotation\n";
        ms += "    float rot_tl_u = uv_l, rot_tl_v = uv_t;\n";
        ms += "    float rot_tr_u = uv_r, rot_tr_v = uv_t;\n";
        ms += "    float rot_bl_u = uv_l, rot_bl_v = uv_b;\n";
        ms += "    float rot_br_u = uv_r, rot_br_v = uv_b;\n";
        ms += "    if (char_rot == 90) {\n";
        ms += "        rot_tl_u = 1.0 - uv_t; rot_tl_v = uv_l;\n";
        ms += "        rot_tr_u = 1.0 - uv_t; rot_tr_v = uv_r;\n";
        ms += "        rot_bl_u = 1.0 - uv_b; rot_bl_v = uv_l;\n";
        ms += "        rot_br_u = 1.0 - uv_b; rot_br_v = uv_r;\n";
        ms += "    } else if (char_rot == 180) {\n";
        ms += "        rot_tl_u = 1.0 - uv_l; rot_tl_v = 1.0 - uv_t;\n";
        ms += "        rot_tr_u = 1.0 - uv_r; rot_tr_v = 1.0 - uv_t;\n";
        ms += "        rot_bl_u = 1.0 - uv_l; rot_bl_v = 1.0 - uv_b;\n";
        ms += "        rot_br_u = 1.0 - uv_r; rot_br_v = 1.0 - uv_b;\n";
        ms += "    } else if (char_rot == 270) {\n";
        ms += "        rot_tl_u = uv_b; rot_tl_v = 1.0 - uv_l;\n";
        ms += "        rot_tr_u = uv_b; rot_tr_v = 1.0 - uv_r;\n";
        ms += "        rot_bl_u = uv_t; rot_bl_v = 1.0 - uv_l;\n";
        ms += "        rot_br_u = uv_t; rot_br_v = 1.0 - uv_r;\n";
        ms += "    }\n";
        ms += "\n";

        // ── 颜色解包 ─────────────────────────────────────────────
        ms += "    const vec4 char_color = unpackUnorm4x8(cs.text_color);\n";
        ms += "\n";

        // ── 写入 6 个 mesh 顶点（三角形列表，匹配 sl_l2r 绕序）────
        // Tri 1: TL(0), BL(1), TR(2)
        // Tri 2: TR(3), BL(4), BR(5)
        ms += "    // Triangle 1: TL, BL, TR\n";
        ms += "    gl_MeshVerticesEXT[base_vid + 0u].gl_Position = viewport.ortho_matrix * vec4(pos_tl_x, pos_tl_y, 0.0, 1.0);\n";
        ms += "    gl_MeshVerticesEXT[base_vid + 1u].gl_Position = viewport.ortho_matrix * vec4(pos_bl_x, pos_bl_y, 0.0, 1.0);\n";
        ms += "    gl_MeshVerticesEXT[base_vid + 2u].gl_Position = viewport.ortho_matrix * vec4(pos_tr_x, pos_tr_y, 0.0, 1.0);\n";
        // Triangle 2: TR, BL, BR
        ms += "    gl_MeshVerticesEXT[base_vid + 3u].gl_Position = viewport.ortho_matrix * vec4(pos_tr_x, pos_tr_y, 0.0, 1.0);\n";
        ms += "    gl_MeshVerticesEXT[base_vid + 4u].gl_Position = viewport.ortho_matrix * vec4(pos_bl_x, pos_bl_y, 0.0, 1.0);\n";
        ms += "    gl_MeshVerticesEXT[base_vid + 5u].gl_Position = viewport.ortho_matrix * vec4(pos_br_x, pos_br_y, 0.0, 1.0);\n";
        ms += "\n";

        // ── 三角形索引 ───────────────────────────────────────────
        ms += "    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 0u] = uvec3(base_vid, base_vid + 1u, base_vid + 2u);\n";
        ms += "    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 1u] = uvec3(base_vid + 3u, base_vid + 4u, base_vid + 5u);\n";
        ms += "\n";

        // ── UV varying ───────────────────────────────────────────
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::UV0))
        {
            ms += "    fragUV0[base_vid + 0u] = vec2(rot_tl_u, rot_tl_v);  // TL\n";
            ms += "    fragUV0[base_vid + 1u] = vec2(rot_bl_u, rot_bl_v);  // BL\n";
            ms += "    fragUV0[base_vid + 2u] = vec2(rot_tr_u, rot_tr_v);  // TR\n";
            ms += "    fragUV0[base_vid + 3u] = vec2(rot_tr_u, rot_tr_v);  // TR\n";
            ms += "    fragUV0[base_vid + 4u] = vec2(rot_bl_u, rot_bl_v);  // BL\n";
            ms += "    fragUV0[base_vid + 5u] = vec2(rot_br_u, rot_br_v);  // BR\n";
        }

        // ── 颜色 varying ─────────────────────────────────────────
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::Color))
        {
            ms += "    for (int i = 0; i < 6; i++)\n";
            ms += "        fragVertexColor[base_vid + uint(i)] = char_color;\n";
        }

        // ── DataIndexID varying ──────────────────────────────────
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::DataIndexID))
        {
            ms += "    const uint data_id = ResolveDataIndexID(gl_InstanceIndex);\n";
            ms += "    for (int i = 0; i < 6; i++)\n";
            ms += "        fragDataIndexID[base_vid + uint(i)] = data_id;\n";
        }

        // StyleID varying（flat per-vertex 样式索引 → FS 查 sbo_char_style）
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::StyleID))
        {
            ms += "    for (int i = 0; i < 6; i++)\n";
            ms += "        fragStyleID[base_vid + uint(i)] = style_id;\n";
        }
    }
}
