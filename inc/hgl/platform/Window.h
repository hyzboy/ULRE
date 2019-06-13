#ifndef HGL_GRAPH_WINDOW_INCLUDE
#define HGL_GRAPH_WINDOW_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/platform/InputDevice.h>
#include<hgl/graph/vulkan/VK.h>

namespace hgl
{
    class Window
    {
        VK_NAMESPACE::Device *device;
        virtual VkSurfaceKHR CreateSurface(VkInstance)=0;

    protected:

        uint width,height;
        bool full_screen;

        OSString win_name;

        bool active;
        bool is_close;
        bool is_min;

        bool key_push[kbRangeSize];

    protected:

        virtual bool MessageProc()=0;
        virtual bool WaitMessage()=0;

    public:

        virtual void ProcMouseMove      (int x,int y)               {SafeCallEvent(OnMouseMove,     (x,y));}
        virtual void ProcMouseWheel     (int x,int y, int v,int h)  {SafeCallEvent(OnMouseWheel,    (x,y,v,h));}
        virtual void ProcMouseDown      (int x,int y,uint mb)       {SafeCallEvent(OnMouseDown,     (x,y,mb));}
        virtual void ProcMouseUp        (int x,int y,uint mb)       {SafeCallEvent(OnMouseUp,       (x,y,mb));}
        virtual void ProcMouseDblClick  (int x,int y,uint mb)       {SafeCallEvent(OnMouseDblClick, (x,y,mb));}

        //virtual void ProcJoystickDown   (uint);
        //virtual void ProcJoystickPress  (uint);
        //virtual void ProcJoystickUp     (uint);

        virtual void ProcKeyDown    (KeyboardButton);
        virtual void ProcKeyPress   (KeyboardButton kb){SafeCallEvent(OnKeyPress,(kb));}
        virtual void ProcKeyUp      (KeyboardButton);

        virtual void ProcChar       (os_char ch){SafeCallEvent(OnChar,(ch));}

        virtual void ProcResize     (uint,uint);

        virtual void ProcActive     (bool);
        virtual void ProcClose      ();

    public:

        uint GetWidth()const{return width;}
        uint GetHeight()const{return height;}

    public:

        DefEvent(void,OnMouseMove       ,(int,int));
        DefEvent(void,OnMouseWheel      ,(int,int, int,int));
        DefEvent(void,OnMouseDown       ,(int,int,uint));
        DefEvent(void,OnMouseUp         ,(int,int,uint));
        DefEvent(void,OnMouseDblClick   ,(int,int,uint));

        //DefEvent(void,OnJoystickDown    ,(uint));
        //DefEvent(void,OnJoystickPress ,(uint));
        //DefEvent(void,OnJoystickUp    ,(uint));

        DefEvent(void,OnKeyDown ,(KeyboardButton));
        DefEvent(void,OnKeyPress,(KeyboardButton));
        DefEvent(void,OnKeyUp   ,(KeyboardButton));

        DefEvent(void,OnChar    ,(os_char));

        DefEvent(void,OnResize  ,(uint,uint));

        DefEvent(void,OnActive  ,(bool));
        DefEvent(void,OnClose   ,());

    public:

        Window(const OSString &wn)
        {
            device=nullptr;
            width=height=0;
            full_screen=false;
            win_name=wn;
            active=false;
            is_close=true;
            is_min=false;
            hgl_zero(key_push);
        }
        virtual ~Window();

        virtual bool Create(uint,uint)=0;
        virtual bool Create(uint,uint,uint)=0;
        virtual void Close()=0;

                bool IsMin()const{return is_min;}
                bool IsClose()const{return is_close;}
                bool IsVisible()const{return (!is_close)&&width&&height;}

        virtual void SetCaption(const OSString &)=0;

        virtual void Show()=0;
        virtual void Hide()=0;

        virtual void ToMinWindow()=0;
        virtual void ToMaxWindow()=0;

        virtual void SetSystemCursor(bool){}

        virtual bool Update();

    public:

        VK_NAMESPACE::Device *  CreateRenderDevice(VK_NAMESPACE::Instance *,const VK_NAMESPACE::PhysicalDevice *pd=nullptr);
        void                    CloseRenderDevice();
    };//class Window

    Window *CreateRenderWindow(const OSString &win_name);

    void InitNativeWindowSystem();
}//namespace hgl
#endif//HGL_GRAPH_WINDOW_INCLUDE
