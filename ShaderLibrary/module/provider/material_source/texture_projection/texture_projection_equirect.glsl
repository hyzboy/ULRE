// @ulre begin
// @ulre name texture_projection_equirect
// @ulre kind Shared
// @ulre priority 0
// @ulre uses bindless_textures
// @ulre end
// 等距柱状（equirectangular）UV → 球面方向。
//
// 有些材质源贴图不吃 uv，而吃**方向**（cubemap、八面体展开图）。而无光照模板的
// 表面输入里没有法线/视线，只有 uv0；不过球体几何的 uv 本身就是等距柱状的
// （InlineGeometry/HexSphere：u = atan2(n.y,n.x)/2π+0.5、v = asin(n.z)/π+0.5），
// 于是可以从 uv 精确反算出顶点属性空间的方向 —— 它就是几何顶点法线方向，随物体
// 旋转，不需要新增 varying，也不需要改主着色器。
//
// 对不使用等距柱状 uv 的网格，本反算等价于"按 uv 铺图"：仍然连续、无接缝跳变，
// 只是不再等于几何法线方向。纯纹理显示用途下这是可接受的取舍。
#ifndef MATERIAL_TEXTURE_PROJECTION_EQUIRECT_GLSL
#define MATERIAL_TEXTURE_PROJECTION_EQUIRECT_GLSL

#include "module/descriptor/bindless_textures.glsl"

// HexSphere 的 uv 约定（Z-up）：u 绕 +Z 一圈，v 从 -Z 极到 +Z 极。
vec3 MaterialEquirectUvToDirection(vec2 equirectUv)
{
    const float longitude   = (equirectUv.x - 0.5) * 6.283185307179586;
    const float latitude    = (equirectUv.y - 0.5) * 3.141592653589793;
    const float cosLatitude = cos(latitude);
    return vec3(cosLatitude * cos(longitude),
                cosLatitude * sin(longitude),
                sin(latitude));
}

#endif // MATERIAL_TEXTURE_PROJECTION_EQUIRECT_GLSL
