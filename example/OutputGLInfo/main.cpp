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

void output_ogl_value(const char *name,const GLenum gl_caps)
{
    GLint params;

    glGetIntegerv(gl_caps, &params);

    std::cout << name << ": " << params << std::endl;
}

#define OUTPUT_OGL_VALUE(name) output_ogl_value("GL_" #name,GL_##name)

void output_ogl_values()
{
    OUTPUT_OGL_VALUE(MAX_VERTEX_ATTRIBS);
    OUTPUT_OGL_VALUE(MAX_VERTEX_ATTRIB_BINDINGS);

    OUTPUT_OGL_VALUE(MAX_TEXTURE_COORDS);
    OUTPUT_OGL_VALUE(MAX_VERTEX_TEXTURE_IMAGE_UNITS);       //单个顶点着色器能访问的纹理单元数
    OUTPUT_OGL_VALUE(MAX_TEXTURE_IMAGE_UNITS);              //单个片段着色器能访问的纹理单元数
    OUTPUT_OGL_VALUE(MAX_GEOMETRY_TEXTURE_IMAGE_UNITS);     //单个几何着色器能访问的纹理单元数
    OUTPUT_OGL_VALUE(MAX_COMBINED_TEXTURE_IMAGE_UNITS);     //所有着色器总共能访问的纹理单元数
    OUTPUT_OGL_VALUE(MAX_SAMPLES);
    OUTPUT_OGL_VALUE(MAX_COLOR_ATTACHMENTS);
    OUTPUT_OGL_VALUE(MAX_ARRAY_TEXTURE_LAYERS);
    OUTPUT_OGL_VALUE(MAX_TEXTURE_BUFFER_SIZE);
    OUTPUT_OGL_VALUE(MAX_SHADER_STORAGE_BLOCK_SIZE);

    OUTPUT_OGL_VALUE(MAX_VERTEX_UNIFORM_BLOCKS);
    OUTPUT_OGL_VALUE(MAX_GEOMETRY_UNIFORM_BLOCKS);
    OUTPUT_OGL_VALUE(MAX_FRAGMENT_UNIFORM_BLOCKS);
    OUTPUT_OGL_VALUE(MAX_UNIFORM_BLOCK_SIZE);
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
    output_ogl_values();

    delete win;
    delete device;

    return 0;
}
