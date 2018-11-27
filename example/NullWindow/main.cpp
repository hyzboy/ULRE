#include<hgl/render/RenderDevice.h>
#include<hgl/render/RenderWindow.h>
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

    glClearColor(0,0,0,1);

    while(win->IsOpen())
    {
        win->MakeToCurrent();

        glClear(GL_COLOR_BUFFER_BIT);

        win->SwapBuffer();
        win->PollEvent();
    }

    delete win;
    delete device;

    return 0;
}
