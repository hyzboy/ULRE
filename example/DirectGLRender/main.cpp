#include<hgl/graph/RenderDevice.h>
#include<hgl/graph/RenderWindow.h>
#include<iostream>
#include<GLEWCore/glew.h>
#include<hgl/graph/Shader.h>
#include<hgl/math/Math.h>
#include<hgl/graph/VertexArray.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint screen_width=1280;
constexpr uint screen_height=720;

Matrix4f ortho_2d_matrix;

void InitMatrix()
{
    ortho_2d_matrix=ortho2d(screen_width,screen_height,     //2D画面宽高
                            true);                          //使用top为0,bottom为height的方式
}

constexpr char vertex_shader[]=R"(
#version 330 core

uniform mat4 ModelViewProjectionMatrix;

in vec2 Vertex;
in vec3 Color;

out vec4 FragmentColor;

void main()
{
    vec4 Position;

    FragmentColor=vec4(Color,1.0);

    Position=vec4(Vertex,0.0,1.0);

    gl_Position=Position*ModelViewProjectionMatrix;
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

    if(!shader.SetUniformMatrix4fv("ModelViewProjectionMatrix",ortho_2d_matrix))
        return(false);

    return(true);
}

VB2i *vb_vertex=nullptr;
VB3f *vb_color=nullptr;
VertexArray *va=nullptr;

constexpr int vertex_data[]={100,100,   200,100,    200,200 };
constexpr float color_data[]={1,0,0,    0,1,0,      0,0,1   };

void InitVertexBuffer()
{
    vb_vertex=new VB2i(4,vertex_data);
    vb_color=new VB3f(4,color_data);

    va=new VertexArray(GL_TRIANGLES,        //画三角形
                       2);                  //两个属性

    const int vertex_location=shader.GetAttribLocation("Vertex");               ///<取得顶点流数据输入流对应的shader地址
    const int color_localtion=shader.GetAttribLocation("Color");                ///<取得颜色流数据输入流对应的shader地址

    int binding_index=0;                    //绑定点

    glVertexArrayAttribBinding(va->Get
}

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

    RenderWindow *win=device->Create(screen_width,screen_height,&ws,&rs);

    win->MakeToCurrent();               //切换当前窗口到前台

    InitMatrix();
    if(!InitShader())
    {
        std::cerr<<"init shader failed."<<std::endl;
        return -3;
    }

    InitVertexBuffer();

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
