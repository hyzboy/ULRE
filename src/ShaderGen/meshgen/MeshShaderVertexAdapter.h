// MeshShaderVertexAdapter.h — 顶点索引适配层
//
// mesh shader 无 gl_VertexIndex，提供 MeshVertexIndex 变量 + 宏、
// 顶点流 BDA ref 类型声明 + sbo_vertex_index 垫片宏、
// MeshDrawParams struct + SSBO 声明。

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
        // 或索引查表（VertexIndexRef 大 buffer[index_base + 全局序号]）。
        // 宏必须指向**可变**变量（LoadVertexData 是独立函数，
        // 函数体内不能引用 main 局部变量，且查表需要运行时赋值——不能用常量表达式宏）。
        // 两模式都需要 mesh_draw_params 参数表——由本生成器补声明（见下）。
        ms += "// mesh shader：无 gl_VertexIndex；VertexIndexID = MeshVertexIndex（main 解析）\n";
        ms += "uint MeshVertexIndex;\n";
        ms += "#define VertexIndexID (MeshVertexIndex)\n";
        ms += "\n";
        // 顶点索引查表改走 BDA：基址由 MeshDrawParams 行的 addr_index 携带，
        // sbo_vertex_index 转垫片宏（非索引几何该分支不执行——与旧
        // PARTIALLY_BOUND 语义一致）。

        // mesh per-draw 参数表（IndirectMeshDraw）：per-draw 段偏移经
        // gl_DrawID 查表（间接合批的关键：多命令一次 vkCmdDrawMeshTasksIndirectEXT 提交时
        // 每命令各自的参数只能靠 GPU 侧查表；直接绘制 gl_DrawID=0 → row 0）。
        // 行表本体走 BDA（buffer_reference，MeshDrawParamsRef）——表地址由下方
        // push constant pc_root.addr_mesh_draw_params 携带。
        // 字段顺序与 CPU 侧
        // per-draw 参数行严格一致（24B 头部 + 8×uint64 基址 = 88B）——
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
        // 行表本体走 BDA：地址由 push constant pc_root.addr_mesh_draw_params 携带，
        // shader 经 buffer_reference 解引用（行表 buffer 以 SHADER_DEVICE_ADDRESS usage 创建）。
        // （pc_root block 由 MeshShaderHeaderGen::EmitRootAddressesPushConstant 提前发射——
        //  l2w_ssbo 等模块 include 引用 pc_root，必须先于它们声明。）
        ms += "layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshDrawParamsRef { MeshDrawParams rows[]; };\n";
        ms += "\n";
        // 全局可变参数行：模块函数（orient_world 等经 gl_InstanceIndex 宏）引用
        // first_instance——必须在 main 开头按 gl_DrawID 加载后使用点才生效
        //（跨函数可见，与上方 MeshVertexIndex 同模式）
        ms += "MeshDrawParams draw_params;\n";
        ms += "\n";

        // ── 顶点流 BDA 类型声明（buffer_reference，无描述符无绑定）────────────
        // 每种"流×变体"一个类型名：变体互斥（同流只 include 一个模块），但全部
        // 集中在此声明一次——类型声明不占绑定零开销，重复声明才是编译错误。
        // scalar 布局 + align=16 与 CPU 侧 GetBufferDeviceAddressAligned16 的
        // 基址承诺配对；元素类型/stride 与旧 std430 声明逐字节一致（vec3=12B
        // 紧凑、packed=4B），函数体无需任何改动。
        // 各 s1_* 模块以 #define sbo_vertex_xxx XxxRef(draw_params.addr_xxx) 接入。
        static const char *const kVertexRefDecls[] =
        {
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexIndexRef       { uint data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionRef    { vec3 data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionV2Ref  { vec2 data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionPackedRef { uint data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexUVRef          { vec2 data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexUVPackedRef    { uint data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexNTBRef         { vec3 data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexNTBPackedRef   { uint data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexColorRef       { vec4 data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexColorPackedRef { uint data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexLuminanceRef   { uint data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexTransformIDRef { uint data[]; };\n",
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexSizeRef        { vec2 data[]; };\n",
        };
        for (const char *decl : kVertexRefDecls)
            ms += decl;
        ms += "\n";

        // 顶点索引垫片宏：is_indexed 分支查表（非索引几何 addr_index 为 0，
        // 该分支不执行——与旧 PARTIALLY_BOUND 语义一致）
        ms += "#define sbo_vertex_index VertexIndexRef(draw_params.addr_index)\n";
        ms += "\n";
    }
}
