#pragma once

#include <hgl/graph/BufferAnnotation.h>
#include <hgl/math/VectorTypes.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Projection.h>

namespace hgl::graph
{
    using namespace hgl::math;

    /**
     * 视口 / 正交投影参数块。
     *
     * ⚠ 数据成员必须全 public：本结构是 GPU ABI 的**唯一真源**（`HGL_UBO_BDA`），
     *   生成器据此产出 `ShaderLibrary/ubo/ViewportInfo.glsl` 与 TOML 读写。
     *   私有成员会让它失去 standard-layout ⇒ 无法保证与 GLSL scalar 布局逐字段同偏移。
     *   成员函数不影响布局/平凡复制，可以保留。
     */
    struct HGL_UBO_BDA ViewportInfo
    {
        Matrix4f ortho_matrix;
        Vector2u canvas_resolution;
        Vector2u viewport_resolution;
        Vector2f inv_viewport_resolution;

        ViewportInfo()
        {
            mem_zero(*this);
        }

        void SetViewport(uint w, uint h)
        {
            viewport_resolution.x = w;
            viewport_resolution.y = h;
            inv_viewport_resolution.x = 1.0f / float(w);
            inv_viewport_resolution.y = 1.0f / float(h);
        }

        void SetCanvas(float w, float h)
        {
            canvas_resolution.x = w;
            canvas_resolution.y = h;
            ortho_matrix = math::OrthoMatrix(w, h);
        }

        void Set(uint w, uint h)
        {
            SetViewport(w, h);
            SetCanvas(w, h);
        }

        const uint GetViewportWidth() const { return viewport_resolution.x; }
        const uint GetViewportHeight() const { return viewport_resolution.y; }

        const float GetAspectRatio() const
        {
            return float(canvas_resolution.x) / float(canvas_resolution.y);
        }

        const Vector2u &GetViewport() const
        {
            return viewport_resolution;
        }
    };

    constexpr size_t ViewportInfoBytes = sizeof(ViewportInfo);
}
