// @ulre begin
// @ulre name ntb_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre end
// NTB Space Interface — 定义 NTBSpace 结构体与通用辅助函数
#ifndef NTB_INTERFACE_GLSL
#define NTB_INTERFACE_GLSL

struct NTBSpace
{
    vec3 N; // World Normal
    vec3 T; // World Tangent
    vec3 B; // World Bitangent
};

// 辅助方法：由 Normal 生成相互垂直的正交 T, B (当没有显式 Tangent 矢量输入时)
NTBSpace BuildOrthoNTB(vec3 normal)
{
    NTBSpace ntb;
    ntb.N = normalize(normal);
    vec3 up = abs(ntb.N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    ntb.T = normalize(cross(up, ntb.N));
    ntb.B = cross(ntb.N, ntb.T);
    return ntb;
}

#endif // NTB_INTERFACE_GLSL
