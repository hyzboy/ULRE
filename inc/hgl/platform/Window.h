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

        uint GetWidth()const{return width;}
        uint GetHeight()const{return height;}

    public:

        virtual void OnMouseMove        (int,int){}
        virtual void OnMouseWheel       (int,int, int){}
        virtual void OnMouseDown        (int,int,uint){}
        virtual void OnMouseUp          (int,int,uint){}
        virtual void OnMouseDoubleClick (int,int,uint){}

        //virtual void OnJoystickDown     (uint){}
        //virtual void OnJoystickPress  (uint){}
        //virtual void OnJoystickUp     (uint){}

        virtual void OnKeyDown  (KeyboardButton);
        virtual void OnKeyPress (KeyboardButton){}
        virtual void OnKeyUp    (KeyboardButton);

        virtual void OnChar     (os_char){}

        virtual void OnResize   (uint,uint);

        virtual void OnActive   (bool a){active=a;}
        virtual void OnClose    (){is_close=true;}

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
