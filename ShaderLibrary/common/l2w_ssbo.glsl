// @ulre begin
// @ulre name l2w_ssbo
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// l2w_ssbo.glsl — LocalToWorld 变换表 buffer_reference 统一声明
//
// 需要 pc_root（RootAddresses push constant，MeshShaderHeaderGen 先行发射）
//
// 用法:
//   #include "common/l2w_ssbo.glsl"
//   ...
//   mat4 m = l2w.mats[TransformID];   // l2w 垫片宏 → LocalToWorldDataRef(pc_root.addr_l2w).mats[...]

#ifndef L2W_SSBO_GLSL
#define L2W_SSBO_GLSL

// L2W 表本体：mat4 数组（scalar 布局 stride 64B，与 CPU Matrix4f 逐字节一致；
// buffer_reference_align=16 与 GetBufferDeviceAddressAligned16 承诺配对）。
// 表地址经 pc_root.addr_l2w 下发——无描述符无 set。
layout(buffer_reference, scalar, buffer_reference_align=16) buffer LocalToWorldDataRef
{
    mat4 mats[];
};

#define l2w LocalToWorldDataRef(pc_root.addr_l2w)

#endif // L2W_SSBO_GLSL
