// ShaderLibrary/mesh/stage_common_head.glsl
//
// 网格阶段固定主着色器（mesh/fixed/<形状键>.mesh）前导的上半段：扩展声明、
// RootAddresses push constant、LocalToWorldIndexRef、ResolveTransformID。
//
// 为什么拆成 head / body 两个文件：生成器（src/ShaderGen/meshgen/MeshShaderHeaderGen.h）
// 把 topology 行插在这两段之间，拆开才能让固定源码与生成器输出保持同样的声明顺序，
// 从而 SPIR-V 逐字节可比。
//
// ResolveTransformID 即使形状不用也保留（生成器无条件发射），去掉会改变 SPIR-V。
//
// 本文件不得含结构性 #if 分支。

#extension GL_EXT_mesh_shader : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

layout(push_constant) uniform RootAddresses
{
    uint64_t addr_global_addresses;
    uint64_t addr_world_addresses;
    uint64_t addr_batch_mesh_draw_params;
    uint64_t addr_l2w;
    uint64_t addr_l2w_index;
    uint64_t addr_mtl_data_addrs;
    uint64_t addr_texture_references;
    uint64_t addr_text_char_info;
    uint64_t addr_text_char_style;
    uint64_t addr_text_char_instance;
    uint camera_row;
    uint _pad_camera;
} pc_root;

#define gl_InstanceIndex (draw_params.first_instance + gl_WorkGroupID.y)

layout(buffer_reference, scalar, buffer_reference_align=16) buffer LocalToWorldIndexRef { uint values[]; };
uint ResolveTransformID(uint iid) { return uint(LocalToWorldIndexRef(pc_root.addr_l2w_index).values[iid]); }
