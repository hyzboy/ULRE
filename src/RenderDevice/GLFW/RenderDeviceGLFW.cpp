#include<hgl/render/RenderDevice.h>
#include<GLFW/glfw3.h>
#include<iostream>

namespace hgl
{
    RenderWindow *CreateRenderWindowGLFW(GLFWwindow *,bool is_fullscreen);

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

            const VideoMode *GetCurVideoMode()const override{return cur_video_mode;}
            const ObjectList<VideoMode> &GetVideoModeList()const override{return video_mode_list;}
        };

        VideoMode *ConvertVideoMode(const GLFWvidmode *mode)
        {
            if(!mode)
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

        void SetGLFWWindowHint(const RenderSetup *gs)
        {
            glfwWindowHint(GLFW_SAMPLES,                gs->msaa);

            glfwWindowHint(GLFW_CLIENT_API,             gs->opengl.es?GLFW_OPENGL_ES_API:GLFW_OPENGL_API);

            glfwWindowHint(GLFW_CONTEXT_CREATION_API,   gs->opengl.egl?GLFW_EGL_CONTEXT_API:GLFW_NATIVE_CONTEXT_API);

            if(gs->opengl.es)
            {
            }
            else
            {
                glfwWindowHint(GLFW_OPENGL_PROFILE,         GLFW_OPENGL_CORE_PROFILE);        //核心模式
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,  true);                            //向前兼容模式(无旧特性)
            }

            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT,       gs->opengl.debug);                //调试模式
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,      gs->opengl.major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,      gs->opengl.minor);

            glfwWindowHint(GLFW_VISIBLE,                    true);                            //是否显示

            if(gs->no_use_stencil )glfwWindowHint(GLFW_STENCIL_BITS,  0             ); else
            if(gs->stencil      >0)glfwWindowHint(GLFW_STENCIL_BITS,  gs->stencil   );
            if(gs->alpha        >0)glfwWindowHint(GLFW_ALPHA_BITS,    gs->alpha     );
            if(gs->depth        >0)glfwWindowHint(GLFW_DEPTH_BITS,    gs->depth     );

            if(gs->accum.red    >0)glfwWindowHint(GLFW_ACCUM_RED_BITS,     gs->accum.red    );
            if(gs->accum.green  >0)glfwWindowHint(GLFW_ACCUM_GREEN_BITS,   gs->accum.green  );
            if(gs->accum.blue   >0)glfwWindowHint(GLFW_ACCUM_BLUE_BITS,    gs->accum.blue   );
            if(gs->accum.alpha  >0)glfwWindowHint(GLFW_ACCUM_ALPHA_BITS,   gs->accum.alpha  );
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

        const UTF8String GetName()override
        {
            return UTF8String("GLFW ")+UTF8String(glfwGetVersionString());
        }

        const void GetDisplayList(List<Display *> &disp_list) override
        {
            int count=0;

            GLFWmonitor **ml=glfwGetMonitors(&count);

            for(int i=0;i<count;i++)
                disp_list+=GetDisplayAttrib(ml[i]);
        }

        const Display *                 GetDefaultDisplay() override
        {
            if(!default_display)
                default_display=GetDisplayAttrib(glfwGetPrimaryMonitor());

            return default_display;
        }

    public:

        RenderWindow *Create(int width,int height,const WindowSetup *ws,const RenderSetup *rs) override
        {
            SetGLFWWindowHint(rs);

            glfwWindowHint(GLFW_MAXIMIZED,ws->Maximize);
            glfwWindowHint(GLFW_RESIZABLE,ws->Resize);

            GLFWwindow *win=glfwCreateWindow(   width,
                                                height,
                                                ws->Name,
                                                nullptr,
                                                nullptr);

            if(!win)return(nullptr);

            return(CreateRenderWindowGLFW(win,false));
        }

        RenderWindow *Create(const Display *disp,const VideoMode *vm,const RenderSetup *rs) override
        {
            SetGLFWWindowHint(rs);

            glfwWindowHint(GLFW_RED_BITS,       vm->red);
            glfwWindowHint(GLFW_GREEN_BITS,     vm->green);
            glfwWindowHint(GLFW_BLUE_BITS,      vm->blue);
            glfwWindowHint(GLFW_REFRESH_RATE,   vm->freq);

            GLFWwindow *win=glfwCreateWindow(   vm->width,
                                                vm->height,
                                                "ULRE",
                                                disp?((DisplayGLFW *)disp)->glfw_monitor:nullptr,
                                                nullptr);

            if(!win)return(nullptr);

            return(CreateRenderWindowGLFW(win,true));
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
