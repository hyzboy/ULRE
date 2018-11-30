#include<hgl/graph/RenderDevice.h>
#include<hgl/graph/RenderWindow.h>
#include<iostream>
#include<GLEWCore/glew.h>
#include<hgl/graph/Shader.h>
#include<hgl/math/Math.h>

using namespace hgl;

constexpr uint screen_width=1280;
constexpr uint screen_height=720;

Matrix4f ortho_2d_matrix;

void InitMatrix()
{
    ortho_2d_matrix=ortho2d(screen_width,screen_height,     //2D画面宽高
                            false);                         //Y轴使用底为0顶为1
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
