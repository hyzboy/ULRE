// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/AmbientEnvInfo.h → AmbientEnvInfo
//   布局：kind=ubo bda=1 access=readonly size=176 align=4 ref_align=16
//   说明：块内全为 16 字节对齐字段（env_tex / ldr_param / sh[9]），两端口径一致；地址 = WorldAddresses.addr_ambient_env。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_UBO_AMBIENT_ENV_INFO_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_UBO_AMBIENT_ENV_INFO_GLSL

struct AmbientEnvInfoData
{
    uvec4 env_tex;
    vec4 ldr_param;
    vec4 sh[9];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer AmbientEnvInfoRef
{
    uvec4 env_tex;
    vec4 ldr_param;
    vec4 sh[9];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_UBO_AMBIENT_ENV_INFO_GLSL
