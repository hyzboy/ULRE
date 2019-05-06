#include"XCBWindow.h"
namespace hgl
{
    bool XCBWindow::InitConnection()
    {
        int scr;

        connection=xcb_connect(nullptr,&scr);

        if(!connection||xcb_connection_has_error(connection))
            return(false);

        const xcb_setup_t *setup=xcb_get_setup(connection);
        xcb_screen_iterator_t iter=xcb_setup_roots_iterator(setup);

        while(scr-->0)xcb_screen_next(&iter);

        screen=iter.data;
        return(true);
    }

    XCBWindow::XCBWindow(const UTF8String &wn):Window(wn)
    {
        connection=nullptr;
        screen=nullptr;
        atom_wm_delete_window=nullptr;
    }

    XCBWindow::~XCBWindow()
    {
    }

    bool XCBWindow::Create(uint w,uint h)
    {
        if(w<=0||h<=0)return(false);
        if(!InitConnection())return(false);

        window=xcb_generate_id(connection);

        uint32_t value_mask,value_list[32];

        value_mask=XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        value_list[0] = screen->black_pixel;
        value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE;

        width=w;
        height=h;

        xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0,0, width, height, 0,
                            XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, value_mask, value_list);

        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, 0);

        xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
        atom_wm_delete_window = xcb_intern_atom_reply(connection, cookie2, 0);

        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, (*reply).atom, 4, 32, 1,
                            &(*atom_wm_delete_window).atom);
        free(reply);

        xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window,XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                                win_name.Length(), win_name.c_str());

        xcb_map_window(connection, window);

        const uint32_t coords[] =
        {
            (screen->width_in_pixels-width)/2,
            (screen->height_in_pixels-height)/2
        };
        xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, coords);
        xcb_flush(connection);

        xcb_generic_event_t *e;
        while ((e = xcb_wait_for_event(connection))) {
            if ((e->response_type & ~0x80) == XCB_EXPOSE) break;
        }
    }

    void XCBWindow::Close()
    {
        xcb_destroy_window(connection,window);
        xcb_disconnect(connection);
    }

    void XCBWindow::SetCaption(const OSString &caption)
    {
        win_name=caption;
        xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window,XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                             win_name.Length(), win_name.c_str());
    }

    bool XCBWindow::MessageProc()
    {
        return(true);
    }

    bool XCBWindow::WaitMessage()
    {
        return(true);
    }

    Window *CreateRenderWindow(const UTF8String &win_name)
    {
        return(new XCBWindow(win_name));
    }

    void InitNativeWindowSystem(){}
}//namespace hgl
