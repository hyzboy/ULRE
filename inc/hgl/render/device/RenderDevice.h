#ifndef HGL_RENDER_DEVICE_INCLUDE
#define HGL_RENDER_DEVICE_INCLUDE

#include<hgl/type/_Object.h>
#include<hgl/type/BaseString.h?
namespace hgl
{
    /**
     * 渲染设备基础类<br/>
     * 该类是程序与操作系统或其它系统库的访问交接模块
     */
    class RenderDevice:public _Object
    {
    public:

        RenderDevice()=default;
        virtual ~RenderDevice()=default;

    public:

        virtual UTF8String ToDebugString() override                                                 ///<输出调试用字符串
        {
            return UTF8String(U8_TEXT("RenderDevice(),this is BUG,please override this Function."));
        }
    };//class RenderDevice

    RenderDevice *CreateRenderDeviceGLFW();     ///<创建一个基于GLFW的渲染设备
}//namespace hgl
#endif//HGL_RENDER_DEVICE_INCLUDE
