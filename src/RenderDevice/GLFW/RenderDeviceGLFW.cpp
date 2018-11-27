#include<hgl/render/device/RenderDevice.h>
#include<GLFW/glfw3.h>
#include<iostream>

namespace hgl
{
    namespace
    {
        static RenderDevice *render_device_glfw=nullptr;

        void glfw_error_callback(int err,const char *desc)
        {
            std::cerr<<"glfw return error["<<err<<"] <<"<<desc<<std::endl;
        }

        struct DisplayGLFW:public Display
        {
            GLFWmonitor *glfw_monitor;

            VideoMode *cur_video_mode;
            ObjectList<VideoMode> video_mode_list;

        public:

            const VideoMode *GetCurVideoMode()override{return cur_video_mode;}
            const ObjectList<VideoMode> &GetVideoModeList()override{return video_mode_list;}
        };

        VideoMode *ConvertVideoMode(const GLFWvidmode *mode)
        {
            if(mode)
                return nullptr;

            VideoMode *vm=new VideoMode;

            vm->width   =mode->width;
            vm->height  =mode->height;
            vm->freq    =mode->refreshRate;

            vm->red     =mode->redBits;
            vm->green   =mode->greenBits;
            vm->blue    =mode->blueBits;

            vm->bit     =vm->red+vm->green+vm->blue;

            return vm;
        }

        DisplayGLFW *GetDisplayAttrib(GLFWmonitor *gm)
        {
            DisplayGLFW *m=new DisplayGLFW;

            m->glfw_monitor=gm;
            m->name=glfwGetMonitorName(gm);

            glfwGetMonitorPhysicalSize(gm,&(m->width),&(m->height));
            glfwGetMonitorPos(gm,&(m->x),&(m->y));

            {
                const GLFWvidmode *mode=glfwGetVideoMode(gm);

                m->cur_video_mode=ConvertVideoMode(mode);
            }

            {
                int count;

                const GLFWvidmode *ml=glfwGetVideoModes(gm,&count);

                for(int i=0;i<count;i++)
                    m->video_mode_list.Add(ConvertVideoMode(ml+i));
            }

            return m;
        }
    }

    class RenderDeviceGLFW:public RenderDevice
    {
        DisplayGLFW *default_display=nullptr;

    public:

        RenderDeviceGLFW()
        {
            render_device_glfw=this;
        }

        ~RenderDeviceGLFW()
        {
            glfwTerminate();
            render_device_glfw=nullptr;
        }

        const bool Init()override{return true;}
        const void Close()override{};

        const UTF8String                GetName()override                                           ///<取得设备名称
        {
            return UTF8String("GLFW")+UTF8String(glfwGetVersionString());
        }

        const void GetDisplayList(List<Display *> &disp_list)                                        ///<取得显示屏列表
        {
            int count=0;

            GLFWmonitor **ml=glfwGetMonitors(&count);

            for(int i=0;i<count;i++)
                disp_list+=GetDisplayAttrib(ml[i]);
        }

        const Display *                 GetDefaultDisplay()
        {
            if(!default_display)
                default_display=GetDisplayAttrib(glfwGetPrimaryMonitor());

            return default_display;
        }

    public:

        RenderWindow *Create(int,int,const WindowSetup *,const RenderSetup *) override
        {
            return(nullptr);
        }

        RenderWindow *Create(const Display *,const VideoMode *,const RenderSetup *) override
        {
            return(nullptr);
        }
    };//class RenderDevice

    RenderDevice *CreateRenderDeviceGLFW()
    {
        if(render_device_glfw)
            return(nullptr);

        if(!glfwInit())
            return(nullptr);

        glfwSetErrorCallback(glfw_error_callback);

        return(new RenderDeviceGLFW());
    }
}//namespace hgl
