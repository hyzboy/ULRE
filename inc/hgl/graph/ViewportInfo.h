#pragma once

#include<hgl/math/VectorTypes.h>
#include<hgl/math/MatrixTypes.h>
#include<hgl/math/Projection.h>

namespace hgl::graph
{
    using namespace hgl::math;

    /**
    * 视口信息
    */
    class ViewportInfo
    {
        Matrix4f ortho_matrix;              ///<64 2D正角视图矩阵

        Vector2u canvas_resolution;         ///< 8 画布尺寸(绘图用尺寸)
        Vector2u viewport_resolution;       ///< 8 视图尺寸(显示的实际尺寸,glFragCoord之类用)
        Vector2f inv_viewport_resolution;   ///< 8 视图尺寸的倒数

    public:

        ViewportInfo()
        {
            mem_zero(*this);
        }

        void SetViewport(uint w,uint h)
        {
            viewport_resolution.x=w;
            viewport_resolution.y=h;

            inv_viewport_resolution.x=1.0f/float(w);
            inv_viewport_resolution.y=1.0f/float(h);
        }

        void SetCanvas(float w,float h)
        {
            canvas_resolution.x=w;
            canvas_resolution.y=h;

            ortho_matrix=math::OrthoMatrix(w,h);
        }

        void Set(uint w,uint h)
        {
            SetViewport(w,h);
            SetCanvas(w,h);
        }

    public:

        const uint GetViewportWidth()const{return viewport_resolution.x;}
        const uint GetViewportHeight()const{return viewport_resolution.y;}

        const float GetAspectRatio()const
        {
            return float(canvas_resolution.x)/float(canvas_resolution.y);
        }

        const Vector2u &GetViewport()const
        {
            return viewport_resolution;
        }
    };//class ViewportInfo

    constexpr size_t ViewportInfoBytes=sizeof(ViewportInfo);
}//namespace hgl::graph
