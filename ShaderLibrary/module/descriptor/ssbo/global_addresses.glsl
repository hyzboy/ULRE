// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/GlobalAddresses.h → GlobalAddresses
//   布局：kind=ssbo bda=1 access=readonly size=56 align=8 ref_align=16
//   说明：字段是 64 位 BDA 地址 ⇒ 包含本文件的着色器必须先启用 GL_ARB_gpu_shader_int64 与 GL_EXT_shader_explicit_arithmetic_types_int64；扩展由聚合入口 module/descriptor/ubo/scene_ubo.glsl 统一开启。表按 HGL_FRAME_SLOT_TOTAL 多份，pc_root 指向本帧那一槽。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_GLOBAL_ADDRESSES_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_GLOBAL_ADDRESSES_GLSL

struct GlobalAddressesData
{
    uint64_t addr_mesh_draw_params_pool;
    uint64_t addr_geometry_aabb;
    uint64_t addr_pbr_surface;
    uint64_t addr_emissive_surface;
    uint64_t addr_transmission_surface;
    uint64_t addr_color_palette;
    uint64_t addr_viewport;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer GlobalAddressesRef
{
    uint64_t addr_mesh_draw_params_pool;
    uint64_t addr_geometry_aabb;
    uint64_t addr_pbr_surface;
    uint64_t addr_emissive_surface;
    uint64_t addr_transmission_surface;
    uint64_t addr_color_palette;
    uint64_t addr_viewport;
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_GLOBAL_ADDRESSES_GLSL
