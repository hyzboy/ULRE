#include"WinWindow.h"

namespace hgl
{
    LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    namespace
    {
        constexpr wchar_t WIN_CLASS_NAME[] = L"CMGameEngine/ULRE Window Class";

        bool RegistryWinClass(HINSTANCE hInstance)
        {
            WNDCLASSEXW win_class;

            hgl_zero(win_class);

            win_class.cbSize = sizeof(WNDCLASSEXW);
            win_class.style = CS_HREDRAW | CS_VREDRAW;
            win_class.lpfnWndProc = WindowProc;
            win_class.cbClsExtra = 0;
            win_class.cbWndExtra = 0;
            win_class.hInstance = hInstance;
            win_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
            win_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
            win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
            win_class.lpszMenuName = nullptr;
            win_class.lpszClassName = WIN_CLASS_NAME;
            win_class.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

            return RegisterClassExW(&win_class);
        }
    }//namespace

    bool WinWindow::Create()
    {
        constexpr DWORD win_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU ;

        int win_left, win_top;
        int win_width, win_height;

        {
            RECT win_rect;

            win_rect.left = 0;
            win_rect.right = width;
            win_rect.top = 0;
            win_rect.bottom = height;

            AdjustWindowRectEx(&win_rect, win_style, false, 0); //计算窗口坐标

            win_width = win_rect.right - win_rect.left;
            win_height = win_rect.bottom - win_rect.top;
        }

        if (width && height)
        {
            win_left = (GetSystemMetrics(SM_CXSCREEN) - win_width) / 2;
            win_top = (GetSystemMetrics(SM_CYSCREEN) - win_height) / 2;
        }
        else
        {
            win_left = CW_USEDEFAULT;
            win_top = CW_USEDEFAULT;
        }

        win_hwnd = CreateWindowExW(0,
            WIN_CLASS_NAME,             // class name
            win_name.c_str(),           // app name
            win_style,                  // window style
            win_left,win_top,           // x/y coords
            win_width,                  // width
            win_height,                 // height
            nullptr,                    // handle to parent
            nullptr,                    // handle to menu
            hInstance,                  // hInstance
            nullptr);                   // no extra parameters

        if (!win_hwnd)
        {
            UnregisterClassW(WIN_CLASS_NAME, hInstance);
            return(false);
        }

        win_dc = GetDC(win_hwnd);
        SetWindowLongPtrW(win_hwnd, GWLP_USERDATA, (LONG_PTR)this);
        return(true);
    }

    WinWindow::~WinWindow()
    {
        Close();
    }

    bool WinWindow::Create(uint w, uint h)
    {
        full_screen=false;
        width = w;
        height = h;

        hInstance = GetModuleHandleW(nullptr);

        if (!RegistryWinClass(hInstance))
            return(false);

        if(!Create())
        {
            is_close=true;
            return(false);
        }
        else
        {
            is_close=false;
            return(true);
        }
    }

    bool WinWindow::Create(uint, uint, uint)
    {
        full_screen=true;
        return(false);
    }

    void WinWindow::Close()
    {
        if(win_hwnd)
        {
            if(win_dc)
            {
                ReleaseDC(win_hwnd,win_dc);
                win_dc = nullptr;
            }

            DestroyWindow(win_hwnd);

            win_hwnd = nullptr;
        }

        UnregisterClassW(WIN_CLASS_NAME,hInstance);

        is_close = true;
    }

    void WinWindow::SetCaption(const OSString &caption)
    {
        win_name=caption;
        SetWindowTextW(win_hwnd,caption.c_str());
    }

    void WinWindow::Show()
    {
        ShowWindow(win_hwnd, SW_SHOW);
        SetForegroundWindow(win_hwnd);
        SetFocus(win_hwnd);

        UpdateWindow(win_hwnd);
    }

    void WinWindow::Hide()
    {
        ShowWindow(win_hwnd, SW_HIDE);
        UpdateWindow(win_hwnd);
    }

    void WinWindow::ToMinWindow()
    {
        if(!full_screen)
            ShowWindow(win_hwnd,SW_MINIMIZE);
    }

    void WinWindow::ToMaxWindow()
    {
        if(!full_screen)
            ShowWindow(win_hwnd,SW_MAXIMIZE);
    }

    void WinWindow::SetSystemCursor(bool visible)
    {
        ::ShowCursor(visible);
    }

    bool WinWindow::MessageProc()
    {
        if(PeekMessage(&win_msg,NULL,0,0,PM_REMOVE))
        {
            TranslateMessage(&win_msg);
            DispatchMessage(&win_msg);

            if(win_msg.message==WM_QUIT)
                ProcClose();

            return(true);
        }
        else
        {
            //if(JoyPlugIn)
            //{
            //    JoyInterface.Update();
            //}

            return(false);
        }
    }

    bool WinWindow::WaitMessage()
    {
        return ::WaitMessage();
    }

    Window *CreateRenderWindow(const WideString& win_name)
    {
        return(new WinWindow(win_name));
    }
}//namespace hgl
