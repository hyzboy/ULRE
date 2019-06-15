#include"WinWindow.h"
#include<hgl/platform/InputDevice.h>
#include<Windows.h>

#ifdef _DEBUG
#include<hgl/LogInfo.h>
#endif//_DEBUG

namespace hgl
{
    namespace
    {
        static KeyboardButton KeyConvert[256];
        static void (*WMProc[2048])(WinWindow *,uint32,uint32);                 //消息处理队列

        uint32 GetMouseKeyFlags(uint32 wflags)
        {
            uint32 flag=0;

            if(wflags&MK_LBUTTON)flag|=mbLeft;
            if(wflags&MK_RBUTTON)flag|=mbRight;
            if(wflags&MK_MBUTTON)flag|=mbMid;

            if(wflags&MK_XBUTTON1)flag|=mbX1;
            if(wflags&MK_XBUTTON2)flag|=mbX2;

            if(wflags&MK_SHIFT  )flag|=mbShift;
            if(wflags&MK_CONTROL)flag|=mbCtrl;

            return(flag);
        }

        void InitKeyConvert()
        {
            int i;

            memset(KeyConvert,0,sizeof(KeyConvert));

            KeyConvert[VK_ESCAPE    ]=kbEsc;
            for(i=VK_F1;i<=VK_F12;i++)KeyConvert[i]=(KeyboardButton)(kbF1+i-VK_F1);

            KeyConvert['`'          ]=kbGrave;
            for(i='0';i<='9';i++)KeyConvert[i]=(KeyboardButton)(kb0+i-'0');
            KeyConvert['-'          ]=kbMinus;
            KeyConvert['='          ]=kbEquals;
            KeyConvert['\\'         ]=kbBackSlash;
            KeyConvert[VK_BACK      ]=kbBackSpace;

            KeyConvert[VK_TAB       ]=kbTab;
            KeyConvert['Q'          ]=kbQ;
            KeyConvert['W'          ]=kbW;
            KeyConvert['E'          ]=kbE;
            KeyConvert['R'          ]=kbR;
            KeyConvert['T'          ]=kbT;
            KeyConvert['Y'          ]=kbY;
            KeyConvert['U'          ]=kbU;
            KeyConvert['I'          ]=kbI;
            KeyConvert['O'          ]=kbO;
            KeyConvert['P'          ]=kbP;
            KeyConvert['['          ]=kbLeftBracket;
            KeyConvert[']'          ]=kbRightBracket;

            KeyConvert[VK_CAPITAL   ]=kbCapsLock;
            KeyConvert['A'          ]=kbA;
            KeyConvert['S'          ]=kbS;
            KeyConvert['D'          ]=kbD;
            KeyConvert['F'          ]=kbF;
            KeyConvert['G'          ]=kbG;
            KeyConvert['H'          ]=kbH;
            KeyConvert['J'          ]=kbJ;
            KeyConvert['K'          ]=kbK;
            KeyConvert['L'          ]=kbL;
            KeyConvert[';'          ]=kbSemicolon;
            KeyConvert['\''         ]=kbApostrophe;
            KeyConvert[VK_RETURN    ]=kbEnter;

            KeyConvert[VK_LSHIFT    ]=kbLeftShift;
            KeyConvert['Z'          ]=kbZ;
            KeyConvert['X'          ]=kbX;
            KeyConvert['C'          ]=kbC;
            KeyConvert['V'          ]=kbV;
            KeyConvert['B'          ]=kbB;
            KeyConvert['N'          ]=kbN;
            KeyConvert['M'          ]=kbM;
            KeyConvert[','          ]=kbComma;
            KeyConvert['.'          ]=kbPeriod;
            KeyConvert['/'          ]=kbSlash;
            KeyConvert[VK_RSHIFT    ]=kbRightShift;

            KeyConvert[VK_LCONTROL  ]=kbLeftCtrl;
            KeyConvert[VK_LWIN      ]=kbLeftOS;
            KeyConvert[VK_LMENU     ]=kbLeftAlt;
            KeyConvert[VK_SPACE     ]=kbSpace;
            KeyConvert[VK_RMENU     ]=kbRightAlt;
            KeyConvert[VK_RWIN      ]=kbRightOS;
            KeyConvert[VK_RCONTROL  ]=kbRightCtrl;

            KeyConvert[VK_PAUSE     ]=kbPause;
        //        KeyConvert[VK_CLEAR     ]=kbClear;

            KeyConvert[VK_NUMPAD0   ]=kbNum0;
            KeyConvert[VK_NUMPAD1   ]=kbNum1;
            KeyConvert[VK_NUMPAD2   ]=kbNum2;
            KeyConvert[VK_NUMPAD3   ]=kbNum3;
            KeyConvert[VK_NUMPAD4   ]=kbNum4;
            KeyConvert[VK_NUMPAD5   ]=kbNum5;
            KeyConvert[VK_NUMPAD6   ]=kbNum6;
            KeyConvert[VK_NUMPAD7   ]=kbNum7;
            KeyConvert[VK_NUMPAD8   ]=kbNum8;
            KeyConvert[VK_NUMPAD9   ]=kbNum9;

            KeyConvert[VK_DECIMAL   ]=kbNumDecimal;
            KeyConvert[VK_DIVIDE    ]=kbNumDivide;
            KeyConvert[VK_MULTIPLY  ]=kbNumMultiply;
            KeyConvert[VK_SUBTRACT  ]=kbNumSubtract;
            KeyConvert[VK_ADD       ]=kbNumAdd;

            KeyConvert[VK_UP        ]=kbUp;
            KeyConvert[VK_DOWN      ]=kbDown;
            KeyConvert[VK_LEFT      ]=kbLeft;
            KeyConvert[VK_RIGHT     ]=kbRight;

            KeyConvert[VK_INSERT    ]=kbInsert;
            KeyConvert[VK_DELETE    ]=kbDelete;
            KeyConvert[VK_HOME      ]=kbHome;
            KeyConvert[VK_END       ]=kbEnd;
            KeyConvert[VK_PRIOR     ]=kbPageUp;
            KeyConvert[VK_NEXT      ]=kbPageDown;

            KeyConvert[VK_NUMLOCK   ]=kbNumLock;
            KeyConvert[VK_SCROLL    ]=kbScrollLock;

            //KeyConvert[VK_SHIFT       ]=kbLeftShift;
            //KeyConvert[VK_CONTROL ]=kbLeftCtrl;
            //KeyConvert[VK_MENU        ]=kbLeftAlt;

            KeyConvert[VK_OEM_1     ]=kbSemicolon;
            KeyConvert[VK_OEM_PLUS  ]=kbEquals;
            KeyConvert[VK_OEM_COMMA ]=kbComma;
            KeyConvert[VK_OEM_MINUS ]=kbMinus;
            KeyConvert[VK_OEM_PERIOD]=kbPeriod;
            KeyConvert[VK_OEM_2     ]=kbSlash;
            KeyConvert[VK_OEM_3     ]=kbGrave;
            KeyConvert[VK_OEM_4     ]=kbLeftBracket;
            KeyConvert[VK_OEM_5     ]=kbBackSlash;
            KeyConvert[VK_OEM_6     ]=kbRightBracket;
            KeyConvert[VK_OEM_7     ]=kbApostrophe;
        }

        KeyboardButton ConvertOSKey(uint key)
        {
            if(key>=256)return(kbBeginRange);
            if(KeyConvert[key]==0)return(kbBeginRange);

            if(key==VK_SHIFT)
            {
                if((GetAsyncKeyState(VK_LSHIFT)>>15)&1)
                    return kbLeftShift;
                else
                    return kbRightShift;
            }
            else
            if(key==VK_CONTROL)
            {
                if((GetAsyncKeyState(VK_LCONTROL)>>15)&1)
                    return kbLeftCtrl;
                else
                    return kbRightCtrl;
            }
            if(key==VK_MENU)
            {
                if((GetAsyncKeyState(VK_LMENU)>>15)&1)
                    return kbLeftAlt;
                else
                    return kbRightAlt;
            }

    #ifdef _DEBUG
            if(KeyConvert[key]==0)
            {
                wchar_t name[64];

                ::GetKeyNameTextW(key,name,64);

                LOG_INFO(WideString(L"Unknow Key: " )+WideString(key)
                        +WideString(L" ,name: "     )+WideString(name));
            }
    #endif _DEBUG

            return KeyConvert[key];
        }

        void WMProcDestroy(WinWindow *win,uint32,uint32)
        {
            win->ProcClose();
            PostQuitMessage(0);
        }

        #define WMEF_MOUSE(button,action)   void WMProcMouse##button##action(WinWindow *win,uint32 wParam,uint32 lParam)    \
            {   \
                const int x=LOWORD(lParam); \
                const int y=HIWORD(lParam); \
                \
                win->ProcMouseMove(x,y);  \
                win->ProcMouse##action(x,y,mb##button|GetMouseKeyFlags(wParam));   \
            }

            WMEF_MOUSE(Left,Down);
            WMEF_MOUSE(Left,Up);
            WMEF_MOUSE(Left,DblClick);

            WMEF_MOUSE(Mid,Down);
            WMEF_MOUSE(Mid,Up);
            WMEF_MOUSE(Mid,DblClick);

            WMEF_MOUSE(Right,Down);
            WMEF_MOUSE(Right,Up);
            WMEF_MOUSE(Right,DblClick);

            void WMProcMouseMove(WinWindow *win,uint32 wParam,uint32 lParam)
            {
                win->ProcMouseMove(LOWORD(lParam),HIWORD(lParam));
            }
        #undef WMEF_MOUSE

        #define WMEF2(name) void name(WinWindow *win,uint32 wParam,uint32 lParam)
            WMEF2(WMProcMouseWheel)
            {
                int zDelta=GET_WHEEL_DELTA_WPARAM(wParam);
                uint key=ConvertOSKey(GET_KEYSTATE_WPARAM(wParam));
                
                win->ProcMouseWheel(zDelta,0,key);
            }

            WMEF2(WMProcMouseHWheel)
            {
                int zDelta=GET_WHEEL_DELTA_WPARAM(wParam);
                uint key=ConvertOSKey(GET_KEYSTATE_WPARAM(wParam));
                
                win->ProcMouseWheel(0,zDelta,key);
            }

            WMEF2(WMProcSize)
            {
                win->ProcResize(LOWORD(lParam),HIWORD(lParam));
            }
        #undef WMEF2

        #define WMEF1(name) void name(WinWindow *win,uint32 wParam,uint32)
            WMEF1(WMProcKeyDown)
            {
                win->ProcKeyDown(ConvertOSKey(wParam));
            }

            WMEF1(WMProcKeyUp)
            {
                win->ProcKeyUp(ConvertOSKey(wParam));
            }

            WMEF1(WMProcChar)
            {
                win->ProcChar((wchar_t)wParam);
            }

            WMEF1(WMProcActive)
            {
                //if(JoyPlugIn)
                //    JoyInterface.SetInputActive(wParam);

                win->ProcActive(wParam);
            }
        #undef WMEF1
    }//namespace

    void InitMessageProc()
    {
        memset(WMProc,0,sizeof(WMProc));
        InitKeyConvert();

        //if(joy)
        //    LoadJoystick(win->hInstance,win->hWnd);

    #define WM_MAP(wm,func) WMProc[wm]=func;

        WM_MAP(WM_CLOSE             ,WMProcDestroy);
        WM_MAP(WM_LBUTTONDOWN       ,WMProcMouseLeftDown);
        WM_MAP(WM_LBUTTONUP         ,WMProcMouseLeftUp);
        WM_MAP(WM_LBUTTONDBLCLK     ,WMProcMouseLeftDblClick);
        WM_MAP(WM_MBUTTONDOWN       ,WMProcMouseMidDown);
        WM_MAP(WM_MBUTTONUP         ,WMProcMouseMidUp);
        WM_MAP(WM_MBUTTONDBLCLK     ,WMProcMouseMidDblClick);
        WM_MAP(WM_RBUTTONDOWN       ,WMProcMouseRightDown);
        WM_MAP(WM_RBUTTONUP         ,WMProcMouseRightUp);
        WM_MAP(WM_RBUTTONDBLCLK     ,WMProcMouseRightDblClick);
        WM_MAP(WM_MOUSEWHEEL        ,WMProcMouseWheel);
        WM_MAP(WM_MOUSEHWHEEL       ,WMProcMouseHWheel);
        WM_MAP(WM_MOUSEMOVE         ,WMProcMouseMove);
        WM_MAP(WM_KEYDOWN           ,WMProcKeyDown);
        WM_MAP(WM_KEYUP             ,WMProcKeyUp);
        WM_MAP(WM_SYSKEYDOWN        ,WMProcKeyDown);
        WM_MAP(WM_SYSKEYUP          ,WMProcKeyUp);
        WM_MAP(WM_CHAR              ,WMProcChar);
        WM_MAP(WM_SYSCHAR           ,WMProcChar);
        WM_MAP(WM_ACTIVATE          ,WMProcActive);
        WM_MAP(WM_SIZE              ,WMProcSize);

    #undef WM_MAP
    }

    LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if(uMsg<2048)
            if(WMProc[uMsg])
            {
                WinWindow *win=(WinWindow *)GetWindowLongPtrW(hWnd,GWLP_USERDATA);

                if(win)
                    WMProc[uMsg](win,wParam,lParam);
            }

        return (DefWindowProcW(hWnd, uMsg, wParam, lParam));
    }

    void InitNativeWindowSystem()
    {
        InitMessageProc();
    }
}//namespace hgl
