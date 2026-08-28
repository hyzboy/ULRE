#ifndef HGL_GRAPH_FONT_CHAR_QUAD_CONFIG_H
#define HGL_GRAPH_FONT_CHAR_QUAD_CONFIG_H

#include <cstdint>

namespace hgl::graph::mtl
{
    // CharQuad mesh shader 每工作组字符数上限（mesh shader invocation 数）。
    //
    // CPU 端 dispatch（TextRenderPipeline）与 ShaderGen 端（GenericMaterialBuilder
    // 生成的 group_size）必须一致——本处为唯一真源，禁止在任一侧硬编码。
    //
    // 数值依据：CharQuad 每线程生成 4 顶点（1 quad = 2 三角形，顶点复用——
    // TR/BL 由两个三角形共享），Vulkan 规范保证 maxMeshOutputVertices ≥ 256，
    // 64 × 4 = 256 = 上限（旧版 6 顶点/字符时只能 42 × 6 = 252）。
    constexpr uint32_t TEXT_CHARQUAD_MAX_INVOCATIONS = 64u;
}

#endif//HGL_GRAPH_FONT_CHAR_QUAD_CONFIG_H
