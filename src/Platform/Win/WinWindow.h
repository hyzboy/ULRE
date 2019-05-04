#pragma once
#include<hgl/platform/Window.h>
#include<Windows.h>

namespace hgl
{
    /**
    * Windows平台窗口实现
    */
    class WinWindow:public Window
    {
        HINSTANCE hInstance = nullptr;
        HWND win_hwnd = nullptr;
        HDC win_dc = nullptr;

        MSG win_msg;

    protected:

        bool Create();

        void OnClose() override{Close();}

    public:

        using Window::Window;
        ~WinWindow();

        HINSTANCE GetHInstance(){return hInstance;}
        HWND GetHWnd(){return win_hwnd;}

    public:

        bool Create(uint w, uint h) override;
        bool Create(uint, uint, uint) override;
        void Close() override;

        void SetCaption(const OSString &caption) override;

        void Show() override;
        void Hide() override;

        void ToMinWindow() override;
        void ToMaxWindow() override;

        void SetSystemCursor(bool visible) override;

        bool MessageProc() override;
        bool WaitMessage() override;
    };//class WinWindow :public Window
}//namespace win
