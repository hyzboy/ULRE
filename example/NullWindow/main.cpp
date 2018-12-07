#include<hgl/graph/RenderDevice.h>
#include<hgl/graph/RenderWindow.h>
#include<iostream>
#include<GLFW/glfw3.h>

using namespace hgl;

int main(void)
{
    RenderDevice *device=CreateRenderDeviceGLFW();

    if(!device)
    {
        std::cerr<<"Create RenderDevice(GLFW) failed."<<std::endl;
        return -1;
    }

    if(!device->Init())
    {
        std::cerr<<"Init RenderDevice(GLFW) failed."<<std::endl;
        return -2;
    }

    WindowSetup ws;

    ws.Name=U8_TEXT("Null Window");

    RenderSetup rs;

    RenderWindow *win=device->Create(1280,720,&ws,&rs);

    win->Show();

    glClearColor(0,0,0,1);              //设置清屏颜色

    while(win->IsOpen())
    {
        win->MakeToCurrent();           //切换当前窗口到前台

        glClear(GL_COLOR_BUFFER_BIT);   //清屏

        win->SwapBuffer();              //交换前后台显示缓冲区
        win->PollEvent();               //处理窗口事件
    }

    delete win;
    delete device;

    return 0;
}
