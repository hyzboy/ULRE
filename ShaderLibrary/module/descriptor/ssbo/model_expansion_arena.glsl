// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/ModelExpansionArena.h → ModelExpansionArena
//   布局：kind=ssbo bda=1 access=readwrite size=48 align=16 ref_align=16
//   说明：GPU 模型展开竞技场（世界私有，地址 WorldAddresses.addr_model_expansion）：**读写**块（展开侧写 flags/l2w 段指针的消费者是剔除 shader 自己不算写入，但段内矩阵写的是 L2W 池，故本块用 HGL_SSBO_ROW 保持 readwrite 权限以免后续追加可变列时再改一次 ABI）。按 HGL_FRAME_SLOT_TOTAL 多份，pc_root 已指向本帧槽。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_EXPANSION_ARENA_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_EXPANSION_ARENA_GLSL

struct ModelExpansionArenaData
{
    uint flags;
    uint l2w_base_row;
    uint l2w_row_capacity;
    uint bucket_base_count;
    uint64_t bucket_base_addr;
    uint64_t reserved1;
    uint64_t reserved2;
    uint64_t reserved3;
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer ModelExpansionArenaRef
{
    ModelExpansionArenaData arena;
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_EXPANSION_ARENA_GLSL
