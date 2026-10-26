// ShaderLibrary/mesh/stage_common_head.glsl
//
// 网格阶段固定主着色器（mesh/shape/<形状名>.mesh）前导的上半段：扩展声明、
// RootAddresses push constant、LocalToWorldIndexRef、ResolveTransformID。
//
// 共享前缀与 topology 声明分开，供不同形状 master 复用同一套引擎 ABI 声明。
//
// ResolveTransformID 是共享阶段接口的一部分；移除前需确认所有形状 master 均不再引用。
//
// 本文件不得含结构性 #if 分支。

#extension GL_EXT_mesh_shader : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

#include "common/interstage_locations.glsl"

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
