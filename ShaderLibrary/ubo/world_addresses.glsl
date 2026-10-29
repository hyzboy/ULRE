// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/WorldAddresses.h → WorldAddresses
//   布局：bda=1 access=readonly align=16
//   说明：字段是 64 位 BDA 地址 ⇒ 包含本文件的着色器必须先启用
//         GL_ARB_gpu_shader_int64 / GL_EXT_shader_explicit_arithmetic_types_int64。
//         表按 HGL_FRAME_SLOT_TOTAL 多份，pc_root 指向本帧那一槽。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_UBO_WORLD_ADDRESSES_GLSL
#define HGL_SHADERLIB_UBO_WORLD_ADDRESSES_GLSL

struct WorldAddressesData
{
    uint64_t addr_camera_info;
    uint64_t addr_shadow;
    uint64_t addr_entity_gpu;
    uint64_t addr_l2w;
    uint64_t addr_entity_render_ids;
    uint64_t addr_lights;
    uint64_t addr_ambient_env;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer WorldAddressesRef
{
    uint64_t addr_camera_info;
    uint64_t addr_shadow;
    uint64_t addr_entity_gpu;
    uint64_t addr_l2w;
    uint64_t addr_entity_render_ids;
    uint64_t addr_lights;
    uint64_t addr_ambient_env;
};

#endif // HGL_SHADERLIB_UBO_WORLD_ADDRESSES_GLSL
