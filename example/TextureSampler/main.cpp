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

constexpr uint screen_width=640;
constexpr uint screen_height=640;

GLuint texture_id=0;
GLuint sampler_id=0;

namespace hgl
{
    namespace graph
    {
        bool LoadTGATexture(const std::string &filename,GLuint &tex_id);
        GLuint CreateSamplerObject(GLint min_filter,GLint mag_filter,GLint clamp,const GLfloat *border_color);
    }
}

constexpr char vertex_shader[]=R"(
#version 330 core

in vec2 Vertex;
in vec2 TexCoord;

out vec2 FragmentTexCoord;

void main()
{
    FragmentTexCoord=TexCoord;

    gl_Position=vec4(Vertex,0.0,1.0);
})";

constexpr char fragment_shader[]=R"(
#version 330 core

uniform sampler2D TextureLena;

in vec2 FragmentTexCoord;
out vec4 FragColor;

void main()
{
    FragColor=texture(TextureLena,FragmentTexCoord);
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
VB2f *vb_texcoord=nullptr;
VertexArray *va=nullptr;

constexpr float vertex_data[]={ -0.75f, 0.75f,
                                -0.75f,-0.75f,
                                 0.75f, 0.75f,
                                 0.75f,-0.75f
};

constexpr float texcoord_data[]={   -0.25,-0.25,
                                    -0.25, 1.25,
                                     1.25,-0.25,
                                     1.25, 1.25   };


void BindVBO2VAO(const int vao,const int binding_index,const int shader_location,VertexBufferBase *vb)
{
    glVertexArrayAttribBinding(vao,shader_location,binding_index);
    glVertexArrayAttribFormat(vao,shader_location,vb->GetComponent(),vb->GetDataType(),GL_FALSE,0);
    glEnableVertexArrayAttrib(vao,shader_location);
    glVertexArrayVertexBuffer(vao,binding_index,vb->GetBufferIndex(),0,vb->GetStride());
}

void InitVertexBuffer()
{
    vb_vertex=new VB2f(4,vertex_data);
    vb_texcoord=new VB2f(4,texcoord_data);

    va=new VertexArray(GL_TRIANGLE_STRIP,       //画三角形条
                        2);                     //两个属性

    const int vertex_location=shader.GetAttribLocation("Vertex");              ///<取得顶点数据输入流对应的shader地址
    const int texcoord_location=shader.GetAttribLocation("TexCoord");             ///<取得纹理坐标数据输入流对应的shader地址

    int binding_index=0;                    //绑定点

    const int vao=va->GetVAO();

    va->SetVertexBuffer(vb_vertex);
    va->AddVertexAttribBuffer(vb_texcoord);

    BindVBO2VAO(vao,binding_index,vertex_location,vb_vertex);
    ++binding_index;
    BindVBO2VAO(vao,binding_index,texcoord_location,vb_texcoord);
}

bool InitTexture()
{
    if(!LoadTGATexture("lena.tga",texture_id))
        return(false);

    const GLfloat border_color[4]={1,1,0,1};

    sampler_id=CreateSamplerObject(GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_CLAMP_TO_BORDER,border_color);

    int texture_location=shader.GetUniformLocation("TextureLena");

    glBindTextureUnit(0,texture_id);
    glBindSampler(0,sampler_id);

    shader.SetUniform1i(texture_location,0);

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

    if(!InitTexture())
        return -4;

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
