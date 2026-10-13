// @ulre begin
// @ulre name RenderItemResolve
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// RenderItemResolve.glsl — 4-ID 行形状的唯一真源
//
// 只留两件活着的东西：
//   · `RenderItemDescriptor`（16 B uvec4，对应 CPU 端 graph::RenderItemDescriptor）
//   · `RenderItemBufferRef`（该形状的 buffer_reference 视图；实体 4-ID 表按 EntityID.index 寻址它）
//
// 2026-10-14：本文件原有的 BDA 寻址 + 直通/二级索引解析（含两级缓冲视图与两个地址宏）已随
// 「CPU/GPU 双剔除遗留」整删 —— 那套解析全仓零调用者（其启用开关字段亦已不存在）。
// 证据与定论见 doc/analysis/ecs-audit-2026-10/03-renderitem-descriptor-two-tables.md。
// 文件名的 Resolve 属历史命名债（模块名被 ShaderGen 注册表引用，改名另批）。

#ifndef RENDER_ITEM_RESOLVE_GLSL
#define RENDER_ITEM_RESOLVE_GLSL

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

// 16 字节图元描述符 (4-ID)
struct RenderItemDescriptor
{
    uint transform_id;
    uint geometry_id;
    uint material_id;
    uint texture_id;
};

// 4-ID 行表视图 (16B scalar 对齐)
layout(buffer_reference, scalar, buffer_reference_align=16) buffer RenderItemBufferRef
{
    RenderItemDescriptor items[];
};

#endif // RENDER_ITEM_RESOLVE_GLSL
