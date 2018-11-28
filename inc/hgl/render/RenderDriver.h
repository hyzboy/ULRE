#ifndef HGL_RENDER_DRIVER_INCLUDE
#define HGL_RENDER_DRIVER_INCLUDE

#include<hgl/render/RenderStatus.h>
namespace hgl
{
    /**
     * 渲染驱动
     * 用于对真实渲染API的交接管理
     */
    class RenderDriver
    {
    private:

        RenderStatus current_status;

    public:

        virtual void SetCurStatus(const RenderStatus &)=0;

        virtual void ClearColorBuffer()=0;
        virtual void ClearDepthBuffer()=0;
    };//class RenderDriver
}//namespace hgl
#endif//HGL_RENDER_DRIVER_INCLUDE
