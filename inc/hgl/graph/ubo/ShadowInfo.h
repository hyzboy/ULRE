#pragma once

#include <hgl/math/Vector.h>
#include <hgl/math/Matrix.h>

namespace hgl::graph
{
    using namespace math;

    constexpr uint32_t kMaxShadowCascades = 4;

    /**
     * 单个 CSM 级联的数据契约。
     *
     * shadow_vp / shadow_params / shadow_map_size / inv_shadow_map_size /
     * shadow_tex 与现有单级阴影字段保持相同含义。其余字段为滚动缓存
     * 预留，Step 1 只生产并传递这些元数据，尚不改变渲染路径。
     *
     * cascade_params:
     *   x = split near，y = split far，z = blend width，w = flags
     * cache_origin:
     *   x/y = 光空间中已 snap 的缓存原点，z/w = 每个 texel 对应的光空间尺寸
     * cache_offset:
     *   x/y = 物理贴图中的环形偏移（texel），z/w = 保留
     * cache_valid_rect:
     *   x/y = 当前有效区域左上角（texel），z/w = 有效区域尺寸（texel）
     */
    struct ShadowCascadeInfo
    {
        Matrix4f shadow_vp              = Matrix4f(1.0f);
        Vector4f shadow_params          = Vector4f(0.002f, 1.5f, 0.12f, 0.0f);
        Vector2f shadow_map_size        = Vector2f(1024.0f, 1024.0f);
        Vector2f inv_shadow_map_size    = Vector2f(1.0f / 1024.0f, 1.0f / 1024.0f);
        Vector4u shadow_tex             = Vector4u(0);

        Vector4f cascade_params         = Vector4f(0.0f);
        Vector4f cache_origin           = Vector4f(0.0f);
        Vector4u cache_offset           = Vector4u(0);
        Vector4u cache_valid_rect       = Vector4u(0);
    };

    static_assert(sizeof(ShadowCascadeInfo) == 176, "ShadowCascadeInfo size must be 176 bytes for std140 ABI");

    /**
     * CPU 侧的滚动缓存状态。
     *
     * 该结构不直接上传 GPU，负责保存每级级联的滚动基准和失效信息。
     * valid 为 0 时必须整级联重建；scene_revision 变化可用于静态场景
     * 变更后的缓存失效。
     */
    struct ShadowCascadeCacheState
    {
        Vector2f snapped_origin     = Vector2f(0.0f);
        Vector2f texel_world_size   = Vector2f(0.0f);
        Vector4u scroll_offset      = Vector4u(0);
        Vector4u valid_rect         = Vector4u(0);
        uint32_t scene_revision     = 0;
        uint32_t generation         = 0;
        uint32_t valid              = 0;
    };

    /**
     * 阴影参数 UBO（SceneBinding::Shadow / Set 0, Binding 5）
     *
     * 对齐标准 std140 布局：
     *   - shadow_vp: 光照空间 View-Projection 矩阵（world -> light clip）
     *   - shadow_params: x = bias (0.002), y = pcf_radius (1.5), z = darkness (0.12), w = unused
     *   - shadow_map_size: 贴图尺寸 (如 1024, 1024)
     *   - inv_shadow_map_size: 贴图像素大小 (如 1/1024, 1/1024)
     *   - shadow_tex: x = bindless 纹理句柄 (1-based, 0=未绑定), y = 贴图 array layer, z/w = 保留
     *   - csm_params: x = 当前级联数量，y = CSM flags，z/w = 保留
     *   - cascades: 固定数量的 CSM 级联数据
     */
    struct ShadowInfo
    {
        Matrix4f shadow_vp              = Matrix4f(1.0f);
        Vector4f shadow_params          = Vector4f(0.002f, 1.5f, 0.12f, 0.0f);
        Vector2f shadow_map_size        = Vector2f(1024.0f, 1024.0f);
        Vector2f inv_shadow_map_size    = Vector2f(1.0f / 1024.0f, 1.0f / 1024.0f);
        Vector4u shadow_tex             = Vector4u(0);

        Vector4u csm_params             = Vector4u(0);
        ShadowCascadeInfo cascades[kMaxShadowCascades];
    };

    static_assert(sizeof(ShadowInfo) == 832, "ShadowInfo size must be 832 bytes for std140 ABI");
}
