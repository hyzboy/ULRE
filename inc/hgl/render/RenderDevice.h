#ifndef HGL_RENDER_DEVICE_INCLUDE
#define HGL_RENDER_DEVICE_INCLUDE

#include<hgl/type/_Object.h>
#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>

namespace hgl
{
    /**
     * 显示模式数据结构
     */
    struct VideoMode
    {
        int width;                      ///<宽
        int height;                     ///<高
        int bit;                        ///<色彩位数
        int freq;                       ///<刷新率

        int red;                        ///<红色位数
        int green;                      ///<绿色位数
        int blue;                       ///<蓝色位数
    };//struct VideoMode

    /**
     * 显示屏数据结构
     */
    struct Display
    {
        UTF8String name;                                                                        ///<显示屏名称
        int width,height;                                                                       ///<显示屏尺寸(单位:毫米)
        int x,y;                                                                                ///<多显示屏时的排列位置

    public:

        virtual const VideoMode *GetCurVideoMode()const=0;
        virtual const ObjectList<VideoMode> &GetVideoModeList()const=0;
    };

    struct WindowSetup
    {
        UTF8String Name;                                                                            ///<窗口标题

//         OSString IconFilename;                                                                   ///<图标文件名称
//         OSString CursorFilename;                                                                 ///<光标文件名称
        bool Edge       =true;                                                                      ///<是否显示边框

        bool SysMenu    =true;                                                                      ///<是否显示系统菜单
        bool Right      =false;                                                                     ///<窗口是否使用靠右风格

        bool Resize     =false;                                                                     ///<窗口大小是否可调整
        bool Minimize   =false;                                                                     ///<窗口是否可以最小化
        bool Maximize   =false;                                                                     ///<窗口是否可以最大化

        bool TopMost    =false;                                                                     ///<永远在最上面
        bool AppTaskBar =true;                                                                      ///<程序项在任务栏显示
    };

    /**
     * 渲染设备
     */
    struct RenderSetup
    {
        uint alpha;                                                                             ///<Alpha缓冲区位深度,默认8位
        uint depth;                                                                             ///<Depth缓冲区位深度,默认24
        uint stencil;                                                                           ///<Stencil缓冲区位深度,默认8,不使用请写0

        bool no_use_stencil;                                                                    ///<不使用stencil缓冲区

        struct
        {
            uint red;                                                                           ///<Accum缓冲区红色位深度,默认0
            uint green;                                                                         ///<Accum缓冲区绿色位深度,默认0
            uint blue;                                                                          ///<Accum缓冲区蓝色位深度,默认0
            uint alpha;                                                                         ///<Accum缓冲区Alpha位深度,默认0
        }accum;

        uint msaa;                                                                              ///<多重采样级别(全屏抗矩齿级别)

        struct
        {
            struct
            {
                bool nicest;                                                                    ///<高质量贴图压缩，默认为真
            }compress;

            bool rect;                                                                          ///<是否启用矩形贴图，默认为否
            bool npot;                                                                          ///<是否启用非2次幂贴图，默认为否

            float lod_bias;                                                                     ///<默认纹理LOD Bias(默认0)
            float max_anistropy;                                                                ///<纹理最大各向异性过滤值比例(使用0.0-1.0，默认1)
        }texture;

        struct
        {
            bool debug=true;

            bool es=false;
            bool egl=false;

            uint major=3;
            uint minor=3;
        }opengl;
    };

    class RenderWindow;

    /**
     * 渲染设备基础类<br/>
     * 该类是程序与操作系统或其它系统库的访问交接模块
     */
    class RenderDevice:public _Object
    {
    public:

        RenderDevice()=default;
        virtual ~RenderDevice()=default;

        virtual const bool                      Init()=0;                                           ///<初始化渲染设备
        virtual const void                      Close()=0;                                          ///<关闭渲染设备

        virtual const UTF8String                GetName()=0;                                        ///<取得设备名称

        virtual const void                      GetDisplayList(List<Display *> &)=0;                ///<取得显示屏列表
        virtual const Display *                 GetDefaultDisplay()=0;                              ///<取得默认显示屏

    public:

        virtual RenderWindow *Create(int,int,const WindowSetup *,const RenderSetup *)=0;            ///<创建一个窗口渲染设备
        virtual RenderWindow *Create(const Display *,const VideoMode *,const RenderSetup *)=0;      ///<创建一个全屏渲染设备
    };//class RenderDevice

    RenderDevice *CreateRenderDeviceGLFW();     ///<创建一个基于GLFW的渲染设备
}//namespace hgl
#endif//HGL_RENDER_DEVICE_INCLUDE
