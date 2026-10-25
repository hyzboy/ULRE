// root_bindings.glsl — 引擎级固定前导，跨全部 MaterialDomain / 全部 pass 逐字一致。
//
// 此前这段内容由 ShaderGen 在 `BuildMaterialStageDocument` 里以
// Extension / Resource / Define 文档块的形式**注入**到每个 stage 文档前面，
// 因此从材质文本、从 `.mi.toml`、从任何 shader 源文件里都看不到它。
// 废除 ShaderGen 后它是**物理固定**的一段文本：所有 domain 主着色器第一行
// `#include` 本文件即可，不再有任何"注入"概念。
//
// 唯一性约束：`pc_root` 是 push constant 的**唯一**声明点。任何主着色器都
// 不得再声明 `RootAddresses`，否则会得到重复的 push constant 块。
//
// ⚠ 字段顺序 = C++ 侧 `kRootAddressesFieldNames / kRootAddressesFieldTypes`
// 的顺序（inc/hgl/graph/ShaderBufferSources.h），改动必须两边同步，否则整条
// BDA 寻址错位。此文件是物理固定文本，不参与任何生成。
#ifndef HGL_DOMAIN_ROOT_BINDINGS_GLSL
#define HGL_DOMAIN_ROOT_BINDINGS_GLSL

#extension GL_EXT_mesh_shader : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

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

// 材质实例行寻址。`.mi.toml`（MaterialRecipe）在运行时只是一行 BDA 记录，
// 从不参与编译——这是"材质实例不需要独立 SPV"的物理依据。
//
// ⚠ 下面这 4 个字段是**手写**的（域固定架构下不再由发射器遍历 X 列表生成）。
// 字段名与顺序必须与 inc/hgl/graph/ShaderBufferSources.h 的
// `HGL_MATERIAL_INSTANCE_ADDRESSES_FIELD_LIST` 完全一致——顺序决定行内偏移，
// 错位即静默读错字段。该不变量由 TestCSMIncrementalPass Test 14 ②
// （CheckMaterialInstanceAddressesRowMirror）跨文件逐字段比对守住。
struct MaterialInstanceAddresses
{
    uint payload_index;
    uint texture_reference_index;
    float shadow_bias_multiplier;
    uint shadow_flags;
};
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialInstanceAddressesRef
{
    MaterialInstanceAddresses values[];
};

#define HGL_MATERIAL_SHADOW_FLAG_NO_RECEIVE 1u

#endif // HGL_DOMAIN_ROOT_BINDINGS_GLSL
