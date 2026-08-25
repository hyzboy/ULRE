// MeshShaderAssembler.h — 通用 mesh shader 生成器
//
// 生成 GLSL 骨架，顶点输出走 mesh 图元：
//   - 通用模式（默认）：每线程 1 顶点，直通到 gl_MeshVerticesEXT（模拟 VS 行为）
//   - Line quad 模式：每线程 1 线段，展开成 quad（2 三角形）
//
// 复用现有 s1_* 模块（LoadVertexData 读 SSBO）——通过宏把 gl_VertexIndex
// 替换为 gl_LocalInvocationIndex（mesh shader 无 gl_VertexIndex）。
//
// 输出契约：varying/descriptor/Stage1/2/3 结构，
// 由 CompileCompositorMaterial 注入 binding_preamble + 索引表声明。

#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <vulkan/vulkan.h>
#include <string>
#include "VertexVaryingConfig.h"   // mtl::VertexVaryingConfig

namespace hgl::graph::mtl
{
    // MeshShaderAssembler — 生成 mesh stage GLSL
    //
    // 参数说明：
    //   max_invocations — threadgroup 大小（受设备 maxMeshWorkGroupSizeX 限制）
    //
    // 通用模式输出拓扑：triangle list（每 3 连续顶点 1 三角形）
    // Line 模式输出拓扑：triangle list（每线段 4 顶点 2 三角形）
    enum class MeshShaderMode
    {
        VertexPassthrough,   // 每线程 1 顶点（默认，模拟 VS）
        LineQuad,            // 每线程 1 线段 → quad
        CharQuad,            // 每线程 1 字符实例 → quad（6 顶点 2 三角形）
    };

    inline std::string GenerateMeshShader(
        const VertexShaderNodeConfig &node_cfg,
        const VertexVaryingConfig    &varying_cfg,
        VkFormat                      position_format,
        const std::string            &shader_lib_path,
        const MeshShaderMode          mode = MeshShaderMode::VertexPassthrough,
        const uint32_t                max_invocations = 64,
        const std::string            &resolved_input_glsl = {},
        const std::string            &provider_glsl = {},
        const ValueArray<InterStageSemanticContractEntry>
            *resolved_stage_interface = nullptr,
        const PrimitiveType           primitive_type = PrimitiveType::Triangles)
    {
        std::string ms;
        ms.reserve(3072);

        // ── 拓扑/容量 ──────────────────────────────────────────────────────
        uint32_t verts_per_inv = 1;
        uint32_t prims_per_inv = 0;

        switch (mode)
        {
        case MeshShaderMode::VertexPassthrough:
            verts_per_inv = 1;
            prims_per_inv = 0;   // 三角形由 3 线程组填索引
            break;
        case MeshShaderMode::LineQuad:
            verts_per_inv = 4;
            prims_per_inv = 2;
            break;
        case MeshShaderMode::CharQuad:
            verts_per_inv = 6;    // 6 vertices per character (2 triangles)
            prims_per_inv = 2;    // 2 triangles per character
            break;
        }

        const uint32_t max_vertices    = max_invocations * verts_per_inv;
        const uint32_t max_primitives  = max_invocations * ((prims_per_inv > 0) ? prims_per_inv : 1);

        // ── Header ─────────────────────────────────────────────────────────
        // 460：glslang 仅在 GLSL 4.60 起（或 GL_ARB_shader_draw_parameters）在
        // mesh 阶段符号表声明 gl_DrawID——450 下报 undeclared identifier
        ms += "#version 460\n";
        ms += "#extension GL_EXT_mesh_shader : require\n";
        ms += "#extension GL_EXT_scalar_block_layout : require\n";
        ms += "\n";
        ms += "layout(local_size_x = ";
        ms += std::to_string(max_invocations);
        ms += ") in;\n";
        ms += "layout(triangles, max_vertices = ";
        ms += std::to_string(max_vertices);
        ms += ", max_primitives = ";
        ms += std::to_string(max_primitives);
        ms += ") out;\n";
        ms += "\n";

        // ── Descriptor macros ──────────────────────────────────────────────
        ms += "#include \"common/descriptor_macros.glsl\"\n";

        const bool needs_camera = (node_cfg.projection == ProjectionMode::WorldCameraVP ||
                                   node_cfg.orientation == OrientationMode::CameraFacingFree ||
                                   node_cfg.orientation == OrientationMode::CameraFacingAxisY);
        const bool needs_viewport = (node_cfg.projection == ProjectionMode::OrthoViewport ||
                                     node_cfg.projection == ProjectionMode::OrthoThenLocalToWorld ||
                                     node_cfg.scale     == ScaleMode::FixedPixelSize);
        const bool needs_l2w = (node_cfg.orientation == OrientationMode::World ||
                                node_cfg.orientation == OrientationMode::CameraFacingFree ||
                                node_cfg.orientation == OrientationMode::CameraFacingAxisY);

        if (needs_camera || needs_viewport)
        {
            if (needs_camera)
            {
                ms += "#include \"ubo/camera_info.glsl\"\n";
                ms += "SCENE_CAMERA_UBO;\n";
            }
            if (needs_viewport)
            {
                ms += "#include \"ubo/viewport_info.glsl\"\n";
                ms += "SCENE_VIEWPORT_UBO;\n";
            }
        }

        if (needs_l2w)
        {
            ms += "#include \"common/l2w_ssbo.glsl\"\n";
            ms += "L2W_SSBO;\n";
        }

        ms += "\n";

        // ── Stage 1: 顶点输入（SSBO）───────────────────────────────────────
        // mesh shader：无 gl_VertexIndex。VertexIndexID 映射到可变全局 MeshVertexIndex，
        // 由 main 开头解析：非索引直通（全局顶点序号 = gl_WorkGroupID.x*group+局部）
        // 或索引查表（sbo_vertex_index[index_base + 全局序号]）——与 VS 的 s1_index
        // is_indexed 分支语义一致。宏必须指向**可变**变量（LoadVertexData 是独立函数，
        // 函数体内不能引用 main 局部变量，且查表需要运行时赋值——不能用常量表达式宏）。
        // 跳过 s1_index（其 VertexIndexID 变量声明与宏冲突、gl_VertexIndex 在 mesh 不存在）。
        // 两模式都需要 mesh_draw_params 参数表——由本生成器补声明（见下）。
        ms += "// mesh shader：无 gl_VertexIndex；VertexIndexID = MeshVertexIndex（main 解析）\n";
        ms += "uint MeshVertexIndex;\n";
        ms += "#define VertexIndexID (MeshVertexIndex)\n";
        ms += "#define HGL_INDEX_LOADER_DEFINED\n";
        ms += "\n";
        // 顶点索引 SSBO（is_indexed 查表用；非索引几何不写 descriptor——PARTIALLY_BOUND 安全，
        // 与 VS 的 s1_index 声明一致：layout 恒有 binding 8）
        ms += "layout(set=VERTEX_SET, binding=VERTEX_INDEX_BINDING, std430) readonly buffer VertexIndexData\n";
        ms += "{ uint data[]; } sbo_vertex_index;\n";
        ms += "\n";

        // mesh per-draw 参数表（IndirectMeshDraw）：替代 push constant——per-draw 段偏移经
        // gl_DrawID 查表（间接合批的关键：多命令一次 vkCmdDrawMeshTasksIndirectEXT 提交时
        // 每命令各自的参数只能靠 GPU 侧查表；直接绘制 gl_DrawID=0 → row 0）。
        // 声明用 MESH_DRAW_PARAMS_SET/BINDING 宏（descriptor_macros.glsl 默认值 +
        // CompileCompositorMaterial binding_preamble 注入实际值）。字段顺序与 CPU 侧
        // per-draw 参数行严格一致（std430 全 4 字节成员，24B 无 padding）。
        // gl_DrawID 在 mesh 阶段合法（GLSL_EXT_mesh_shader：vertex/task/mesh 输入）。
        {
            ms += "struct MeshDrawParams\n";
            ms += "{\n";
            ms += "    uint index_base;\n";
            ms += "    uint vertex_base;\n";
            ms += "    uint is_indexed;\n";
            ms += "    uint total_vertices;\n";
            ms += "    float viewport_height;\n";
            ms += "    uint first_instance;\n";
            ms += "};\n";
            ms += "layout(set=MESH_DRAW_PARAMS_SET, binding=MESH_DRAW_PARAMS_BINDING, std430) readonly buffer MeshDrawParamsData\n";
            ms += "{ MeshDrawParams rows[]; } sbo_draw_params;\n";
            ms += "\n";
            // 全局可变参数行：模块函数（orient_world 等经 gl_InstanceIndex 宏）引用
            // first_instance——必须在 main 开头按 gl_DrawID 加载后使用点才生效
            //（跨函数可见，与上方 MeshVertexIndex 同模式）
            ms += "MeshDrawParams pc_vertex_index;\n";
            ms += "\n";
        }

        if (mode != MeshShaderMode::CharQuad)
        {
            if (!resolved_input_glsl.empty())
            {
                ms += resolved_input_glsl;
                if (resolved_input_glsl.back() != '\n')
                    ms += "\n";
            }
            else
            {
                // 非索引直通：Position 从 SSBO 读（s1_position_* 模块）
                // Vec2Position → s1_position_vec2；Vec3Position → s1_position_vec3
                VertexInputMode effective_input = node_cfg.input;
                if (position_format == VK_FORMAT_R32G32_SFLOAT)
                    effective_input = VertexInputMode::Vec2Position;
                else if (position_format == VK_FORMAT_R32G32B32_SFLOAT ||
                         position_format == VK_FORMAT_R32G32B32A32_SFLOAT)
                    effective_input = VertexInputMode::Vec3Position;

                switch (effective_input)
                {
                case VertexInputMode::Vec2Position:
                    ms += "#include \"vertex/s1_position_vec2.glsl\"\n";
                    break;
                case VertexInputMode::Vec3Position:
                default:
                    ms += "#include \"vertex/s1_position_vec3.glsl\"\n";
                    break;
                }
            }

            // 额外顶点属性（color/UV 等 provider 模块）
            if (!provider_glsl.empty())
            {
                ms += provider_glsl;
                if (provider_glsl.back() != '\n') ms += "\n";
            }
        }

        // MaterialColorPalette UBO（palette 材质）
        if (varying_cfg.emit_vertex_color_from_palette)
        {
            ms += "#include \"ubo/color_palette.glsl\"\n";
            ms += "SCENE_COLOR_PALETTE_UBO;\n";
        }

        ms += "\n";

        // mesh shader 无 gl_InstanceIndex（VS 专属内置）——实例索引 = first_instance + gl_WorkGroupID.y：
        // DrawMeshTasks(gx, gy) 的 gl_WorkGroupID.y = 实例内序号（0..gy-1），first_instance 是
        // Draw 的 firstInstance（l2w_index_rows 按整批 item 序号写，VS 的 gl_InstanceIndex =
        // firstInstance + 实例内序号 与之对应——mesh 必须补 first_instance 偏移，否则
        // 多 draw_batch 场景所有 batch 都读 values[0] → 模型集中到第一个 transform）。
        // 宏覆盖所有后续模块（orient_world 的 ResolveTransformID(gl_InstanceIndex) 等）。
        ms += "#define gl_InstanceIndex (pc_vertex_index.first_instance + gl_WorkGroupID.y)\n";
        ms += "\n";

        // ── Stage 2: 位置映射 ─────────────────────────────────────────────
        // CharQuad 在 main() 中直接用 viewport.ortho_matrix 做坐标变换，不需要 Stage2/3 模块
        if (mode != MeshShaderMode::CharQuad)
        {
            const char *stage2_module = VertexNodeConfigResolver::GetMappingModulePath(node_cfg);
            if (stage2_module)
                ms += "#include \"" + std::string(stage2_module) + "\"\n";
            else
            {
                ms += "// TODO: TerrainGrid Stage2 not yet implemented\n";
                ms += "vec4 GetLocalPos() { return vec4(0.0); }\n";
            }

            ms += "\n";

            // ── Stage 3: 变换策略 ──────────────────────────────────────────────
            if (varying_cfg.use_transform_id_attr)
                ms += "#define HGL_L2W_FROM_VERTEX_ATTR\n";

            ms += "#include \"" + std::string(VertexNodeConfigResolver::GetStage3ModulePath(node_cfg)) + "\"\n";
            ms += "\n";
        }

        // ── Varying 输出（per-vertex 数组，mesh shader 要求）──────────────
        // varying 声明为数组（按顶点索引）
        ValueArray<InterStageSemanticContractEntry> adapted_stage_interface;
        MaterialStageInterfaceDiagnostic stage_interface_diagnostic{};
        if (!resolved_stage_interface)
        {
            MaterialVertexVaryingConfig material_varying{};
            material_varying.emit_data_index_id   = varying_cfg.emit_data_index_id;
            material_varying.emit_vertex_color    = varying_cfg.emit_vertex_color;
            material_varying.emit_uv0             = varying_cfg.emit_uv0;
            material_varying.emit_world_pos       = varying_cfg.emit_world_pos;
            material_varying.emit_world_normal    = varying_cfg.emit_world_normal;
            material_varying.emit_luminance       = varying_cfg.emit_luminance;
            material_varying.emit_frag_direction  = varying_cfg.emit_frag_direction;
            material_varying.emit_vertex_color_from_palette =
                varying_cfg.emit_vertex_color_from_palette;
            material_varying.emit_style_id      = varying_cfg.emit_style_id;
            if (!BuildMaterialStageInterface(material_varying,
                                             adapted_stage_interface,
                                             stage_interface_diagnostic))
                return {};
            resolved_stage_interface = &adapted_stage_interface;
        }

        // mesh shader 的 per-vertex varying 必须是数组（按顶点索引访问）。
        // 不依赖 BuildGLSLInterStageDeclaration（它生成标量 out）——按语义直接生成数组声明。
        const std::string array_size_str = std::to_string(max_vertices);

        for (int i = 0; i < resolved_stage_interface->GetCount(); ++i)
        {
            const auto &entry = (*resolved_stage_interface)[i];

            // 语义 → GLSL 类型 + 变量名
            const char *type_name = nullptr;
            const char *var_name  = nullptr;

            switch (entry.semantic)
            {
            case InterStageSemantic::DataIndexID: type_name = "flat uint"; var_name = "fragDataIndexID"; break;
            case InterStageSemantic::Color:       type_name = "vec4";       var_name = "fragVertexColor"; break;
            case InterStageSemantic::UV0:         type_name = "vec2";       var_name = "fragUV0"; break;
            case InterStageSemantic::WorldPosition: type_name = "vec3";     var_name = "fragWorldPos"; break;
            case InterStageSemantic::WorldNormal: type_name = "vec3";       var_name = "fragWorldNormal"; break;
            case InterStageSemantic::Luminance:   type_name = "float";      var_name = "fragLuminance"; break;
            case InterStageSemantic::FragDirection: type_name = "vec3";     var_name = "fragDirection"; break;
            case InterStageSemantic::StyleID:     type_name = "flat uint"; var_name = "fragStyleID"; break;
            default: continue;
            }

            if (!type_name || !var_name)
                continue;

            ms += "layout(location=";
            ms += std::to_string(entry.location);
            ms += ") out ";
            ms += type_name;
            ms += " ";
            ms += var_name;
            ms += "[";
            ms += array_size_str;
            ms += "];\n";
        }

        // CharQuad SSBO 声明必须在全局作用域（void main 之前）
        if (mode == MeshShaderMode::CharQuad)
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

        ms += "\nvoid main()\n{\n";

        // per-draw 参数行加载（两模式统一）：间接合批经 gl_DrawID 定位本命令行，
        // 直接绘制 gl_DrawID=0 → row 0（CPU 侧保证 row 0 = 本 draw 参数）
        ms += "    pc_vertex_index = sbo_draw_params.rows[gl_DrawID];\n";
        ms += "\n";

        // ── 每线程处理 ─────────────────────────────────────────────────────
        switch (mode)
        {
        case MeshShaderMode::VertexPassthrough:
        {
            // 每线程 1 顶点：位置变换 + varying 赋值，直通到 mesh 顶点槽
            // 组内顶点槽位（0 .. max_vertices-1；全局顶点号由 VertexIndexID 宏处理）
            ms += "    const uint vid = gl_LocalInvocationIndex;\n";
            ms += "\n";
            ms += "    const uint total_vertices = pc_vertex_index.total_vertices;\n";
            // 本组有效顶点数（所有 invocation 相同值 → SetMeshOutputsEXT 一致；
            // groupCountX = ceil(total/group_size)，末组起始 <= total，不会 uint 下溢）
            ms += "    const uint verts_this_group = min(";
            ms += std::to_string(max_invocations);
            ms += "u, total_vertices - gl_WorkGroupID.x * ";
            ms += std::to_string(max_invocations);
            ms += "u);\n";
            // 图元数按类型：list = verts/3；fan/strip = verts-2（单组小几何）
            if (primitive_type == PrimitiveType::Fan ||
                primitive_type == PrimitiveType::TriangleStrip)
                ms += "    SetMeshOutputsEXT(verts_this_group, (verts_this_group >= 3u) ? (verts_this_group - 2u) : 0u);\n";
            else
                ms += "    SetMeshOutputsEXT(verts_this_group, verts_this_group / 3u);\n";
            ms += "    if (vid >= verts_this_group)\n";
            ms += "        return;\n";
            ms += "\n";

            // 全局顶点号解析：非索引直通（绘制顺序 = 顶点号）或索引查表（is_indexed）。
            // 与 VS 的 s1_index 分支语义一致（mesh 无 gl_VertexIndex，用跨组全局序号）
            ms += "    MeshVertexIndex = gl_WorkGroupID.x * ";
            ms += std::to_string(max_invocations);
            ms += "u + gl_LocalInvocationIndex;\n";
            ms += "    if (pc_vertex_index.is_indexed != 0u)\n";
            ms += "        MeshVertexIndex = sbo_vertex_index.data[pc_vertex_index.index_base + MeshVertexIndex];\n";
            ms += "\n";

            // LoadVertexData（读 SSBO 单顶点；VertexIndexID 宏 = MeshVertexIndex）
            ms += "    LoadVertexData();\n";

            // 变换（对齐 VS：world pos/normal 一次 GetL2W + camera.vp 投影）
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::DataIndexID))
            {
                // 与 VS 一致：实例 → mtl_data_index_rows 查表（材质数据槽——FS 用它查 mtl.data[].color 等）。
                // gl_InstanceIndex 宏 = first_instance + gl_WorkGroupID.y（跨 draw_batch 正确）
                ms += "    fragDataIndexID[vid] = ResolveDataIndexID(gl_InstanceIndex);\n";
            }
            if (varying_cfg.emit_vertex_color_from_palette)
                ms += "    fragVertexColor[vid] = unpackUnorm4x8(color_palette.color[ColorIndex]);\n";
            else if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::Color))
                ms += "    fragVertexColor[vid] = Color;\n";

            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::UV0))
                ms += "    fragUV0[vid] = TexCoord;\n";
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::Luminance))
                ms += "    fragLuminance[vid] = Luminance;\n";
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::FragDirection))
                ms += "    fragDirection[vid] = normalize(Position);\n";

            const bool emit_world_pos = FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::WorldPosition);
            const bool emit_world_normal = FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::WorldNormal);
            if (emit_world_pos || emit_world_normal)
            {
                ms += "    mat4 _l2w = GetL2W();\n";
                ms += "    vec4 _world_pos = _l2w * GetLocalPos();\n";
                if (emit_world_pos)
                    ms += "    fragWorldPos[vid] = _world_pos.xyz;\n";
                if (emit_world_normal)
                    ms += "    fragWorldNormal[vid] = normalize(mat3(_l2w) * Normal);\n";
                // world-normal 路径投影恒为 WorldCameraVP（与 VS 一致）
                ms += "    gl_MeshVerticesEXT[vid].gl_Position = camera.vp * _world_pos;\n";
            }
            else
            {
                ms += "    gl_MeshVerticesEXT[vid].gl_Position = GetClipPos(GetLocalPos());\n";
            }

            // 三角形索引按图元类型（mesh 输出恒 triangle list——fan/strip 的拓扑由
            // 三角形索引表达；管线侧强制 triangle list）：
            //   Triangles（list）：每 3 连续顶点 1 三角形，vid%3==0 的线程写 (vid,vid+1,vid+2)
            //   TriangleFan：三角形 t = (0, t+1, t+2)，vid 线程写（组内中心 = 组内顶点 0——
            //     仅单组几何正确（quad/平面等小几何），跨组 fan 语义受限）
            //   TriangleStrip：三角形 t = (t, t+1, t+2)，偶数 t 翻转绕序 (t+1, t, t+2)
            if (primitive_type == PrimitiveType::Fan)
            {
                // 图元数 = 顶点数 - 2（fan）
                ms += "    if (verts_this_group >= 3u && (vid + 2u) < verts_this_group)\n";
                ms += "        gl_PrimitiveTriangleIndicesEXT[vid] = uvec3(0u, vid + 1u, vid + 2u);\n";
            }
            else if (primitive_type == PrimitiveType::TriangleStrip)
            {
                // 图元数 = 顶点数 - 2（strip；奇数三角形绕序翻转）
                ms += "    if (verts_this_group >= 3u && (vid + 2u) < verts_this_group)\n";
                ms += "        gl_PrimitiveTriangleIndicesEXT[vid] = ((vid & 1u) == 0u)\n";
                ms += "            ? uvec3(vid, vid + 1u, vid + 2u)\n";
                ms += "            : uvec3(vid + 1u, vid, vid + 2u);\n";
            }
            else
            {
                // 每 3 连续顶点 1 三角形（组内槽位；vid%3==0 的线程填）
                // 非 3 倍数顶点余数不构成三角形（与 VS 的 vertexCount 语义一致）
                ms += "    if ((vid % 3u) == 0u && (vid + 2u) < verts_this_group)\n";
                ms += "        gl_PrimitiveTriangleIndicesEXT[vid / 3u] = uvec3(vid, vid + 1u, vid + 2u);\n";
            }
            break;
        }

        case MeshShaderMode::LineQuad:
        {
            // 每线程 1 线段 → 4 顶点 2 三角形（line-to-quad）
            const std::string group_size = std::to_string(max_invocations);

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
            //     否则 Standard 路径 l2w_index_rows 查表（ResolveTransformID(gl_InstanceIndex)）
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
            // clip 空间偏移——NDC 1 单位 = viewport_height/2 像素：
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
            ms += "    const vec2 offset_ndc = n_ndc * (width_eff / pc_vertex_index.viewport_height);\n";
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

            // varying（per-vertex）
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::DataIndexID))
            {
                // 与 VS 一致：实例 → mtl_data_index_rows 查表（材质数据槽）
                ms += "    const uint data_id = ResolveDataIndexID(gl_InstanceIndex);\n";
                ms += "    fragDataIndexID[vid + 0u] = data_id;\n";
                ms += "    fragDataIndexID[vid + 1u] = data_id;\n";
                ms += "    fragDataIndexID[vid + 2u] = data_id;\n";
                ms += "    fragDataIndexID[vid + 3u] = data_id;\n";
            }
            if (varying_cfg.emit_vertex_color_from_palette)
            {
                ms += "    const vec4 lcolor = unpackUnorm4x8(color_palette.color[color_index]);\n";
                ms += "    fragVertexColor[vid + 0u] = lcolor;\n";
                ms += "    fragVertexColor[vid + 1u] = lcolor;\n";
                ms += "    fragVertexColor[vid + 2u] = lcolor;\n";
                ms += "    fragVertexColor[vid + 3u] = lcolor;\n";
            }
            else if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::Color))
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
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::Luminance))
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
            break;
        }

        case MeshShaderMode::CharQuad:
        {
            // 每线程 1 字符实例 → 6 顶点 2 三角形（字符 quad）
            // 三层数据模型：CharInfo + CharStyle + CharInstance
            const std::string group_size = std::to_string(max_invocations);

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
            ms += "    const int char_height = int(pc_vertex_index.viewport_height);\n";
            ms += "    const int rect_left   = pen_x + mx;\n";
            ms += "    const int rect_top    = pen_y - my + char_height;\n";
            ms += "    const int rect_right  = rect_left + int(mw);\n";
            ms += "    const int rect_bottom = rect_top + int(mh);\n";
            ms += "\n";

            // ── 斜体剪切变形 ──────────────────────────────────────────
            ms += "    const float shear_factor = tan(cs.italic);\n";
            ms += "    const float shear_top = float(mh) * shear_factor;\n";
            ms += "\n";

            // ── UV 解包（half-float → float）─────────────────────────
            ms += "    const vec2 uv_lt = unpackHalf2x16(ci.uv_lt);  // .x=left, .y=top\n";
            ms += "    const vec2 uv_rb = unpackHalf2x16(ci.uv_rb);  // .x=right, .y=bottom\n";
            ms += "    const float uv_l = uv_lt.x;\n";
            ms += "    const float uv_t = uv_lt.y;\n";
            ms += "    const float uv_r = uv_rb.x;\n";
            ms += "    const float uv_b = uv_rb.y;\n";
            ms += "\n";

            // ── 颜色解包 ─────────────────────────────────────────────
            ms += "    const vec4 char_color = unpackUnorm4x8(cs.text_color);\n";
            ms += "\n";

            // ── 写入 6 个 mesh 顶点（三角形列表，匹配 sl_l2r 绕序）────
            // Tri 1: TL(0), BL(1), TR(2)
            // Tri 2: TR(3), BL(4), BR(5)
            ms += "    // Triangle 1: TL, BL, TR\n";
            ms += "    gl_MeshVerticesEXT[base_vid + 0u].gl_Position = viewport.ortho_matrix * vec4(float(rect_left) + shear_top,  float(rect_top),    0.0, 1.0);\n";
            ms += "    gl_MeshVerticesEXT[base_vid + 1u].gl_Position = viewport.ortho_matrix * vec4(float(rect_left),               float(rect_bottom), 0.0, 1.0);\n";
            ms += "    gl_MeshVerticesEXT[base_vid + 2u].gl_Position = viewport.ortho_matrix * vec4(float(rect_right) + shear_top,  float(rect_top),    0.0, 1.0);\n";
            ms += "    // Triangle 2: TR, BL, BR\n";
            ms += "    gl_MeshVerticesEXT[base_vid + 3u].gl_Position = viewport.ortho_matrix * vec4(float(rect_right) + shear_top,  float(rect_top),    0.0, 1.0);\n";
            ms += "    gl_MeshVerticesEXT[base_vid + 4u].gl_Position = viewport.ortho_matrix * vec4(float(rect_left),               float(rect_bottom), 0.0, 1.0);\n";
            ms += "    gl_MeshVerticesEXT[base_vid + 5u].gl_Position = viewport.ortho_matrix * vec4(float(rect_right),              float(rect_bottom), 0.0, 1.0);\n";
            ms += "\n";

            // ── 三角形索引 ───────────────────────────────────────────
            ms += "    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 0u] = uvec3(base_vid, base_vid + 1u, base_vid + 2u);\n";
            ms += "    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 1u] = uvec3(base_vid + 3u, base_vid + 4u, base_vid + 5u);\n";
            ms += "\n";

            // ── UV varying ───────────────────────────────────────────
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::UV0))
            {
                ms += "    fragUV0[base_vid + 0u] = vec2(uv_l, uv_t);  // TL\n";
                ms += "    fragUV0[base_vid + 1u] = vec2(uv_l, uv_b);  // BL\n";
                ms += "    fragUV0[base_vid + 2u] = vec2(uv_r, uv_t);  // TR\n";
                ms += "    fragUV0[base_vid + 3u] = vec2(uv_r, uv_t);  // TR\n";
                ms += "    fragUV0[base_vid + 4u] = vec2(uv_l, uv_b);  // BL\n";
                ms += "    fragUV0[base_vid + 5u] = vec2(uv_r, uv_b);  // BR\n";
            }

            // ── 颜色 varying ─────────────────────────────────────────
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::Color))
            {
                ms += "    for (int i = 0; i < 6; i++)\n";
                ms += "        fragVertexColor[base_vid + uint(i)] = char_color;\n";
            }

            // ── DataIndexID varying ──────────────────────────────────
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::DataIndexID))
            {
                ms += "    const uint data_id = ResolveDataIndexID(gl_InstanceIndex);\n";
                ms += "    for (int i = 0; i < 6; i++)\n";
                ms += "        fragDataIndexID[base_vid + uint(i)] = data_id;\n";
            }

            // StyleID varying（flat per-vertex 样式索引 → FS 查 sbo_char_style）
            if (FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::StyleID))
            {
                ms += "    for (int i = 0; i < 6; i++)\n";
                ms += "        fragStyleID[base_vid + uint(i)] = style_id;\n";
            }
            break;
        }
        }

        ms += "}\n";

        return ms;
    }
}//namespace hgl::graph::mtl
