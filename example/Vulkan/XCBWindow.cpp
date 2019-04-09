#include"Window.h"
#include<xcb/xcb.h>
#include<vulkan/vk_sdk_platform.h>
#include<vulkan/vulkan.h>
#include<vulkan/vulkan_xcb.h>
namespace hgl
{
    namespace graph
    {
        class XCBWindow:public Window
        {
            xcb_connection_t *connection;
            xcb_screen_t *screen;
            xcb_window_t window;
            xcb_intern_atom_reply_t *atom_wm_delete_window;

            xcb_screen_iterator_t iter;
            int scr;

        public:

            XCBWindow(const UTF8String &wn):Window(wn)
            {
                connection=nullptr;
                screen=nullptr;
                atom_wm_delete_window=nullptr;

                scr=-1;
            }

            ~XCBWindow()
            {
            }

            const char *GetVulkanSurfaceExtname()const
            {
                return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
            }

            bool CreateWindow(uint w,uint h) override
            {
                if(w<=0||h<=0)return(false);

                xcb_connect(nullptr,&scr);
            }

            bool CreateFullscreen(uint,uint,uint)override{}
            void CloseWindow()override{}

            void Show()override{}
            void Hide()override{}
        };//class XCBWindow:public Window

        Window *CreateRenderWindow(const UTF8String &win_name)
        {
            return(new XCBWindow(win_name));
        }
    }//namespace graph
}//namespace hgl
