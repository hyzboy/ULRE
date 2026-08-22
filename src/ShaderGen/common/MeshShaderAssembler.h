// MeshShaderAssembler.h — 通用 mesh shader 生成器（彻底废弃 VS 的第一步）
//
// 生成与 GenerateVertexShader 同构的 GLSL 骨架，但顶点输出走 mesh 图元：
//   - 通用模式（默认）：每线程 1 顶点，直通到 gl_MeshVerticesEXT（模拟 VS 行为）
//   - Line quad 模式：每线程 1 线段，展开成 quad（2 三角形）
//
// 复用现有 s1_* 模块（LoadVertexData 读 SSBO）——通过宏把 gl_VertexIndex
// 替换为 gl_LocalInvocationIndex（mesh shader 无 gl_VertexIndex）。
//
// 输出契约与 GenerateVertexShader 一致（varying/descriptor/Stage1/2/3），
// 由 CompileCompositorMaterial 注入 binding_preamble + 索引表声明。

#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <vulkan/vulkan.h>
#include <string>

namespace hgl::graph::mtl
{
    // MeshShaderAssembler — 生成 mesh stage GLSL
    //
    // 参数与 GenerateVertexShader 相同，另加：
    //   max_invocations — threadgroup 大小（受设备 maxMeshWorkGroupSizeX 限制）
    //
    // 通用模式输出拓扑：triangle list（每 3 连续顶点 1 三角形）
    // Line 模式输出拓扑：triangle list（每线段 4 顶点 2 三角形）
    enum class MeshShaderMode
    {
        VertexPassthrough,   // 每线程 1 顶点（默认，模拟 VS）
        LineQuad,            // 每线程 1 线段 → quad
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
        }

        const uint32_t max_vertices    = max_invocations * verts_per_inv;
        const uint32_t max_primitives  = max_invocations * ((prims_per_inv > 0) ? prims_per_inv : 1);

        // ── Header ─────────────────────────────────────────────────────────
        ms += "#version 450\n";
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
        // 两模式都需要 pc_vertex_index push constant——由本生成器补声明（见下）。
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

        // 补 pc_vertex_index push constant 声明（s1_index 被跳过，两模式都需要）
        {
            ms += "layout(push_constant) uniform PC_VertexIndex\n";
            ms += "{\n";
            ms += "    uint index_base;\n";
            ms += "    uint vertex_base;\n";
            ms += "    uint is_indexed;\n";
            ms += "    uint total_vertices;\n";
            ms += "    float viewport_height;\n";
            ms += "    uint first_instance;\n";
            ms += "} pc_vertex_index;\n";
            ms += "\n";
        }

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

        // ── Varying 输出（per-vertex 数组，mesh shader 要求）──────────────
        // 与 GenerateVertexShader 的 varying 声明同构，但声明为数组（按顶点索引）
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

        ms += "\nvoid main()\n{\n";

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
        }

        ms += "}\n";

        return ms;
    }
}//namespace hgl::graph::mtl
