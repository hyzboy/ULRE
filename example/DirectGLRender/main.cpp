#include<hgl/render/RenderDevice.h>
#include<hgl/render/RenderWindow.h>
#include<iostream>
#include<GLFW/glfw3.h>
#include<GL/glext.h>

using namespace hgl;



void draw()
{
    glClearColor(0,0,0,1);          //设置清屏颜色
    glClear(GL_COLOR_BUFFER_BIT);   //清屏
}

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

    ws.Name=U8_TEXT("Direct OpenGL Render");

    RenderSetup rs;

    RenderWindow *win=device->CreateWindow(1280,720,&ws,&rs);

    win->MakeToCurrent();           //切换当前窗口到前台
    win->Show();

    while(win->IsOpen())
    {
        draw();

        win->SwapBuffer();              //交换前后台显示缓冲区
        win->PollEvent();               //处理窗口事件
    }

    delete win;
    delete device;

    return 0;
}
