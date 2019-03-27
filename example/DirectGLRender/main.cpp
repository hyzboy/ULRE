#include<hgl/graph/RenderDevice.h>
#include<hgl/graph/RenderDriver.h>
#include<hgl/graph/RenderWindow.h>
#include<iostream>
#include<hgl/graph/Renderable.h>
#include<hgl/graph/RenderState.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint screen_width=1280;
constexpr uint screen_height=720;

constexpr char vertex_shader[]=R"(
#version 330 core

in vec2 Vertex;
in vec3 Color;

out vec4 FragmentColor;

void main()
{
    FragmentColor=vec4(Color,1.0);

    gl_Position=vec4(Vertex,0.0,1.0);
})";

constexpr char fragment_shader[]=R"(
#version 330 core

in vec4 FragmentColor;
out vec4 FragColor;

void main()
{
    FragColor=vec4(FragmentColor.rgb,1);
})";

Shader shader;

bool InitShader()
{
    if(!shader.AddVertexShader(vertex_shader))
        return(false);

    if(!shader.AddFragmentShader(fragment_shader))
        return(false);

    if(!shader.Build())
        return(false);

    if(!shader.Use())
        return(false);

    return(true);
}

ArrayBuffer *vb_vertex=nullptr;
ArrayBuffer *vb_color=nullptr;
VertexArray *va=nullptr;
Renderable *render_obj=nullptr;
RenderState render_state;

constexpr float vertex_data[]={0.0f,0.5f,   -0.5f,-0.5f,    0.5f,-0.5f };
constexpr float color_data[]={1,0,0,    0,1,0,      0,0,1   };

constexpr GLfloat clear_color[4]=
{
    77.f/255.f,
    78.f/255.f,
    83.f/255.f,
    1.f
};

void InitRenderState()
{
    DepthState *ds=new DepthState();

    ds->clear_depth=true;           //要求清除depth缓冲区
    ds->clear_depth_value=1.0f;     //depth清除所用值

    ds->depth_mask=false;           //关闭depth mask
    ds->depth_test=false;           //关闭depth test

    render_state.Add(ds);

    ColorState *cs=new ColorState();

    cs->clear_color=true;
    memcpy(cs->clear_color_value,clear_color,sizeof(GLfloat)*4);

    render_state.Add(cs);
}

void InitVertexBuffer()
{
    vb_vertex=CreateVBO(VB2f(3,vertex_data));
    vb_color=CreateVBO(VB3f(3,color_data));

    va=new VertexArray();                                                       ///<创建VAO

    const int vertex_location=shader.GetAttribLocation("Vertex");               ///<取得顶点数据输入流对应的shader地址
    const int color_location=shader.GetAttribLocation("Color");                 ///<取得颜色数据输入流对应的shader地址

    va->SetPosition(vertex_location,vb_vertex);
    va->AddBuffer(color_location,vb_color);

    render_obj=new Renderable(GL_TRIANGLES,va);
}

void draw()
{
    render_state.Apply();
    render_obj->Draw();
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

    RenderWindow *win=device->Create(screen_width,screen_height,&ws,&rs);

    win->MakeToCurrent();               //切换当前窗口到前台

    InitOpenGLDebug();                  //初始化OpenGL调试输出

    if(!InitShader())
    {
        std::cerr<<"init shader failed."<<std::endl;
        return -3;
    }

    InitRenderState();
    InitVertexBuffer();

    win->Show();

    while(win->IsOpen())
    {
        draw();

        win->SwapBuffer();              //交换前后台显示缓冲区
        win->PollEvent();               //处理窗口事件
    }

    delete render_obj;
    delete va;
    delete vb_color;
    delete vb_vertex;

    delete win;
    delete device;

    return 0;
}
