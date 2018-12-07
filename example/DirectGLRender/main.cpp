#include<hgl/graph/RenderDevice.h>
#include<hgl/graph/RenderDriver.h>
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

VB2f *vb_vertex=nullptr;
VB3f *vb_color=nullptr;
VertexArray *va=nullptr;

constexpr float vertex_data[]={0.0f,0.5f,   -0.5f,-0.5f,    0.5f,-0.5f };
constexpr float color_data[]={1,0,0,    0,1,0,      0,0,1   };

void BindVBO2VAO(const int vao,const int binding_index,const int shader_location,VertexBufferBase *vb)
{
    glVertexArrayAttribBinding(vao,shader_location,binding_index);
    glVertexArrayAttribFormat(vao,shader_location,vb->GetComponent(),vb->GetDataType(),GL_FALSE,0);
    glEnableVertexArrayAttrib(vao,shader_location);
    glVertexArrayVertexBuffer(vao,binding_index,vb->GetBufferIndex(),0,vb->GetStride());
}

void InitVertexBuffer()
{
    vb_vertex=new VB2f(3,vertex_data);
    vb_color=new VB3f(3,color_data);

    va=new VertexArray(GL_TRIANGLES,        //画三角形
                       2);                  //两个属性

    const int vertex_location=shader.GetAttribLocation("Vertex");               ///<取得顶点流数据输入流对应的shader地址
    const int color_location=shader.GetAttribLocation("Color");                ///<取得颜色流数据输入流对应的shader地址

    int binding_index=0;                    //绑定点

    const int vao=va->GetVAO();

    va->SetVertexBuffer(vb_vertex);
    va->SetColorBuffer(vb_color,HGL_PC_RGB);

    BindVBO2VAO(vao,binding_index,vertex_location,vb_vertex);
    ++binding_index;
    BindVBO2VAO(vao,binding_index,color_location,vb_color);
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

    va->Draw();
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
