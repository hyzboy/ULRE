#include<hgl/render/RenderDevice.h>
#include<hgl/render/RenderWindow.h>
#include<iostream>
#include<GLEWCore/glew.h>

using namespace hgl;

constexpr GLfloat clear_color[4]=
{
    77.f/255.f,
    78.f/255.f,
    83.f/255.f,
    1.f
};

constexpr GLfloat clear_depth=1.0f;

void draw()
{
    glClearBufferfv(GL_COLOR,0,clear_color);
    glClearBufferfv(GL_DEPTH,0,&clear_depth);
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

    ws.Name=U8_TEXT("Direct use \"OpenGL Core API\" Render");

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
