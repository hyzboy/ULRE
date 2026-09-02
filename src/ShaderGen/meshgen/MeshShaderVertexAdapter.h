// MeshShaderVertexAdapter.h — 顶点索引适配层
//
// mesh shader 无 gl_VertexIndex，提供 MeshVertexIndex 变量 + 宏、
// VertexIndex SSBO、MeshDrawParams struct + SSBO 声明。

#pragma once

#include <hgl/graph/ShaderBufferSources.h>
#include <string>

namespace hgl::graph::mtl
{
    // MeshVertexIndex 变量 + 宏 + VertexIndex SSBO + MeshDrawParams struct + SSBO
    inline void EmitVertexAdapter(std::string &ms)
    {
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
        // per-draw 参数行严格一致（std430 全 4 字节成员，24B 无 padding）——
        // 字段名/类型遍历 kMeshDrawParamsField*（ShaderBufferSources.h X 列表单一真源，
        // 与 CPU struct MeshDrawParams 同源，改字段只改那一处）。
        // gl_DrawID 在 mesh 阶段合法（GLSL_EXT_mesh_shader：vertex/task/mesh 输入）。
        ms += "struct MeshDrawParams\n";
        ms += "{\n";
        for (uint32 field_index = 0;
             field_index < kMeshDrawParamsFieldCount;
             ++field_index)
        {
            ms += "    ";
            ms += kMeshDrawParamsFieldGLSLTypes[field_index];
            ms += " ";
            ms += kMeshDrawParamsFieldNames[field_index];
            ms += ";\n";
        }
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
}
