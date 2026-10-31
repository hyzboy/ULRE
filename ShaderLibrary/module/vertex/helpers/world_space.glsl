// @ulre begin
// @ulre name world_space
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// Stage 3 helper: world_space — 世界空间法线的正确变换。
//
// 法线不能直接乘 mat3(l2w)：L2W 含非均匀缩放（或斜切）时，缩放后的法线
// 不再垂直于变换后的表面，PBR 的高光/菲涅耳整体错位。正确变换是逆转置
// 矩阵 transpose(inverse(mat3(l2w)))。
//
// 这里用伴随矩阵（adjugate）等价式替代求逆：
//   adjugate(m) = det(m) · transpose(inverse(m))
// 只差一个标量因子 det —— 归一化之后两者完全一致（det < 0 的镜像变换下
// 两边同时翻转，方向仍然一致）。代价是 3 次叉积 + 1 次 mat3·vec3，
// 比 inverse()（行列式 + 9 个余子式 + 除法）便宜，且逐顶点执行。
//
// 列为 c0/c1/c2 时 adjugate 的列是 (c1×c2, c2×c0, c0×c1)：
// 对角阵 m = diag(sx,sy,sz) 代入得 diag(sy·sz, sz·sx, sx·sy)，
// 归一化后 = diag(1/sx, 1/sy, 1/sz) = 逆转置（对角阵两者相同）✓
//
// 纯函数，不依赖 GetL2W()/任何 UBO —— 矩阵由调用方传入，
// 因此本模块不含资源需求，可被任意 s3 模块 include。

#ifndef HELPER_WORLD_SPACE_GLSL
#define HELPER_WORLD_SPACE_GLSL

vec3 NormalToWorldSpace(mat4 l2w_mat, vec3 local_normal)
{
    mat3 m = mat3(l2w_mat);
    mat3 adjugate = mat3(
        cross(m[1], m[2]),
        cross(m[2], m[0]),
        cross(m[0], m[1]));
    return normalize(adjugate * local_normal);
}

#endif // HELPER_WORLD_SPACE_GLSL
