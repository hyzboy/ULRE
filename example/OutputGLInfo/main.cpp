#include<hgl/graph/RenderDevice.h>
#include<hgl/graph/RenderWindow.h>
#include<iostream>
#include<string.h>
#include<GLEWCore/glew.h>

using namespace hgl;

void draw()
{
    glClearColor(0,0,0,1);          //设置清屏颜色
    glClear(GL_COLOR_BUFFER_BIT);   //清屏
}

void output_ogl_info()
{
    std::cout<<"Vendor:     "<<glGetString(GL_VENDOR)<<std::endl;
    std::cout<<"Renderer:   "<<glGetString(GL_RENDERER)<<std::endl;
    std::cout<<"Version:    "<<glGetString(GL_VERSION)<<std::endl;

    if(!glGetStringi)
        return;

    GLint n=0;

    glGetIntegerv(GL_NUM_EXTENSIONS, &n);

    std::cout<<"Extensions:"<<std::endl;

    for (GLint i=0; i<n; i++)
        std::cout<<"            "<<i<<" : "<<glGetStringi(GL_EXTENSIONS,i)<<std::endl;
}

int main(int argc,char **argv)
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

    ws.Name=U8_TEXT("Draw Triangle");

    RenderSetup rs;

    if(argc>1)
    {
        if(stricmp(argv[1],"core")==0)
            rs.opengl.core=true;
        else
            rs.opengl.core=false;
    }
    else
        rs.opengl.core=false;

    RenderWindow *win=device->Create(1280,720,&ws,&rs);

    win->MakeToCurrent();           //切换当前窗口到前台

    output_ogl_info();

    delete win;
    delete device;

    return 0;
}
