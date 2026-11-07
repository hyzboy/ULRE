// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/InstanceCustomData.h → InstanceCustomData
//   布局：kind=ssbo bda=1 access=readwrite size=16 align=16 ref_align=16
//   说明：逐实例个性化数据行（16B）：custom_color。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_INSTANCE_CUSTOM_DATA_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_INSTANCE_CUSTOM_DATA_GLSL

struct InstanceCustomDataRow
{
    vec4 custom_color;
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer InstanceCustomDataBufferRef
{
    InstanceCustomDataRow rows[];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_INSTANCE_CUSTOM_DATA_GLSL
