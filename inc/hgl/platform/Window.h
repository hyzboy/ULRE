#ifndef HGL_GRAPH_WINDOW_INCLUDE
#define HGL_GRAPH_WINDOW_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/platform/InputDevice.h>
namespace hgl
{
    class Window
    {
    protected:

        uint width,height;
        bool full_screen;

        OSString win_name;

        bool active;
        bool is_close;

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

        virtual void OnResize   (int w,int h){width=w;height=h;}

        virtual void OnActive   (bool a){active=a;}
        virtual void OnClose    (){is_close=true;}

    public:

        Window(const OSString &wn)
        {
            width=height=0;
            win_name=wn;
            active=false;
            is_close=true;
            hgl_zero(key_push);
        }
        virtual ~Window()=default;

        virtual bool Create(uint,uint)=0;
        virtual bool Create(uint,uint,uint)=0;
        virtual void Close()=0;

                bool IsClose()const{return is_close;}
                bool IsVisible()const{return (!is_close)&&width&&height;}

        virtual void SetCaption(const OSString &)=0;

        virtual void Show()=0;
        virtual void Hide()=0;

        virtual void ToMinWindow(){}
        virtual void ToMaxWindow(){}

        virtual void SetSystemCursor(bool){}

        virtual bool Update();
    };//class Window

    Window *CreateRenderWindow(const OSString &win_name);

    void InitNativeWindowSystem();
}//namespace hgl
#endif//HGL_GRAPH_WINDOW_INCLUDE
