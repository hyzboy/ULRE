#pragma once

#include <hgl/math/VectorTypes.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Projection.h>

namespace hgl::graph
{
    using namespace hgl::math;

    class ViewportInfo
    {
        Matrix4f ortho_matrix;
        Vector2u canvas_resolution;
        Vector2u viewport_resolution;
        Vector2f inv_viewport_resolution;

    public:
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
