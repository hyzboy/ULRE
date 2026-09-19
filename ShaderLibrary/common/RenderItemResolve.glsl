// @ulre begin
// @ulre name RenderItemResolve
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// RenderItemResolve.glsl — 全局图元描述符 (4-ID) BDA 寻址与统一解码
//
// 对应 CPU 端 RenderItemDescriptor (16B uvec4)
//
// 支持两种寻址模式：
// 1. 连号直通模式 (Direct Mode: gl_InstanceIndex / firstInstance)
// 2. 间接索引模式 (Indexed Mode: gl_DrawID -> DrawItemIDBuffer -> GlobalRenderItemBuffer)

#ifndef RENDER_ITEM_RESOLVE_GLSL
#define RENDER_ITEM_RESOLVE_GLSL

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"

// 16 字节图元描述符 (4-ID)
struct RenderItemDescriptor
{
    uint transform_id;
    uint geometry_id;
    uint material_id;
    uint texture_id;
};

// 一级全局图元描述符池 (GlobalRenderItemBuffer, 16B scalar 对齐)
layout(buffer_reference, scalar, buffer_reference_align=16) buffer RenderItemBufferRef
{
    RenderItemDescriptor items[];
};

// uvec4 形式视图
layout(buffer_reference, scalar, buffer_reference_align=16) buffer RenderItemUvec4BufferRef
{
    uvec4 items[];
};

// 二级绘制索引表 (DrawItemIDBuffer, uint32_t 紧凑数组, 4B 对齐)
layout(buffer_reference, scalar, buffer_reference_align=4) buffer DrawItemIDBufferRef
{
    uint ids[];
};

// ── 基础 BDA 寻址函数 ──

// 从指定显存物理地址直读 (Direct Mode)
RenderItemDescriptor GetRenderItemDirectFrom(uint64_t addr_table, uint instance_index)
{
    return RenderItemBufferRef(addr_table).items[instance_index];
}

// 从指定显存物理地址二级索引读取 (Indexed Mode)
RenderItemDescriptor GetRenderItemIndexedFrom(uint64_t addr_table, uint64_t addr_ids, uint draw_id)
{
    uint item_id = DrawItemIDBufferRef(addr_ids).ids[draw_id];
    return RenderItemBufferRef(addr_table).items[item_id];
}

// ── 全局 UBO 便捷解引用宏与函数 ──

#define global_render_items RenderItemBufferRef(global_addresses.addr_global_render_items)
#define draw_item_ids       DrawItemIDBufferRef(global_addresses.addr_draw_item_ids)

// 连号直通解析：以 instance_index 直接索引全局表
RenderItemDescriptor ResolveRenderItemDirect(uint instance_index)
{
    return RenderItemBufferRef(global_addresses.addr_global_render_items).items[instance_index];
}

// 间接索引解析：以 draw_id 查二级索引表后索引全局表
RenderItemDescriptor ResolveRenderItemIndexed(uint draw_id)
{
    uint item_id = DrawItemIDBufferRef(global_addresses.addr_draw_item_ids).ids[draw_id];
    return RenderItemBufferRef(global_addresses.addr_global_render_items).items[item_id];
}

// 返回 uvec4 形式
uvec4 ResolveRenderItemDirectUvec4(uint instance_index)
{
    return RenderItemUvec4BufferRef(global_addresses.addr_global_render_items).items[instance_index];
}

uvec4 ResolveRenderItemIndexedUvec4(uint draw_id)
{
    uint item_id = DrawItemIDBufferRef(global_addresses.addr_draw_item_ids).ids[draw_id];
    return RenderItemUvec4BufferRef(global_addresses.addr_global_render_items).items[item_id];
}

#endif // RENDER_ITEM_RESOLVE_GLSL
