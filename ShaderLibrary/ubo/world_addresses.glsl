// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/WorldAddresses.h → WorldAddresses
//   布局：kind=ssbo bda=1 access=readonly size=56 align=8 ref_align=16
//   说明：字段是 64 位 BDA 地址 ⇒ 扩展要求同 GlobalAddresses。表按 HGL_FRAME_SLOT_TOTAL 多份，pc_root 指向本帧那一槽。
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
