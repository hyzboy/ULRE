// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/AmbientEnvInfo.h → AmbientEnvInfo
//   布局：kind=ubo bda=1 access=readonly size=16 align=4 ref_align=16
//   说明：块内只有句柄（env_tex），两端口径一致；地址 = WorldAddresses.addr_ambient_env。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_UBO_AMBIENT_ENV_INFO_GLSL
#define HGL_SHADERLIB_UBO_AMBIENT_ENV_INFO_GLSL

struct AmbientEnvInfoData
{
    uvec4 env_tex;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer AmbientEnvInfoRef
{
    uvec4 env_tex;
};

#endif // HGL_SHADERLIB_UBO_AMBIENT_ENV_INFO_GLSL
