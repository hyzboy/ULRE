#pragma once

#include <hgl/math/Vector.h>
#include <hgl/math/Matrix.h>

namespace hgl::graph
{
    using namespace math;

    /**
     * 阴影参数 UBO（SceneBinding::Shadow / Set 0, Binding 5）
     *
     * 对齐标准 std140 布局：
     *   - shadow_vp: 光照空间 View-Projection 矩阵（world -> light clip）
     *   - shadow_params: x = bias (0.002), y = pcf_radius (1.5), z = darkness (0.12), w = unused
     *   - shadow_map_size: 贴图尺寸 (如 1024, 1024)
     *   - inv_shadow_map_size: 贴图像素大小 (如 1/1024, 1/1024)
     *   - shadow_tex: x = bindless 纹理句柄 (1-based, 0=未绑定), y = 贴图 array layer, z/w = 保留
     */
    struct ShadowInfo
    {
        Matrix4f shadow_vp              = Matrix4f(1.0f);
        Vector4f shadow_params          = Vector4f(0.002f, 1.5f, 0.12f, 0.0f);
        Vector2f shadow_map_size        = Vector2f(1024.0f, 1024.0f);
        Vector2f inv_shadow_map_size    = Vector2f(1.0f / 1024.0f, 1.0f / 1024.0f);
        Vector4u shadow_tex             = Vector4u(0);
    };

    static_assert(sizeof(ShadowInfo) == 112, "ShadowInfo size must be 112 bytes for std140 ABI");
}
