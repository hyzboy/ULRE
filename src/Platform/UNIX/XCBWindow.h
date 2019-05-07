#include<hgl/platform/Window.h>
#include<xcb/xcb.h>
#include<xcb/xcb_atom.h>
namespace hgl
{
    class XCBWindow:public Window
    {
        xcb_connection_t *connection;
        xcb_screen_t *screen;
        xcb_window_t window;
        xcb_intern_atom_reply_t *atom_wm_delete_window;

    private:

        bool InitConnection();
        VkSurfaceKHR CreateSurface(VkInstance) override;

    public:

        XCBWindow(const UTF8String &wn);
        ~XCBWindow();

        bool Create(uint w,uint h)override;
        bool Create(uint,uint,uint)override{}
        void Close()override;

        void SetCaption(const OSString &caption) override;

        void Show()override{}
        void Hide()override{}
        bool MessageProc() override;
        bool WaitMessage() override;
    };//class XCBWindow:public Window
}//namespace hgl

