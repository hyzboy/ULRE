#include<hgl/shadergen/MaterialCreater.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::shadergen;

bool PureColor2DMaterial()
{
    MaterialCreater mc(1,false);                            //一个新材质，1个RT输出，默认使用Vertex/Fragment shader

    //vertex部分
    {
        ShaderCreaterVertex *vsc=mc.GetVS();                //获取vertex shader creater

        //以下代码会被展开为
        /*
            layout(location=?) in vec3 Position;            //位置属性
        */
        vsc->AddInput("vec2","Position");                   //添加一个vec3类型的position属性输入

        vsc->SetShaderCodes(R"(
void main()
{
    gl_Position=vec4(Position,0,1);
})");
    }

    //添加一个名称为ColorMaterial的UBO定义,其内部有一个vec4 color的属性
    //该代码会被展开为
    /*
        struct ColorMaterial
        {
            vec4 color;
        };
     */
    mc.AddStruct("ColorMaterial","vec4 color;");

    //添加一个UBO，该代码会被展开为
    /*
        layout(set=?,binding=?) uniform ColorMaterial mtl;
    */
    mc.AddUBO(  VK_SHADER_STAGE_FRAGMENT_BIT,               //这个UBO出现在fragment shader
                DescriptorSetType::PerMaterial,             //它属于材质合集
                "ColorMaterial",                            //UBO名称为ColorMaterial
                "mtl");                                     //UBO变量名称为mtl

    //fragment部分
    {
        ShaderCreaterFragment *fsc=mc.GetFS();              //获取fragment shader creater

        //以下代码会被展开为
        /*
            layout(location=?) out vec4 Color;              //颜色输出
        */
        fsc->AddOutput("vec4","Color");                     //添加一个vec4类型的color属性输出

        fsc->SetShaderCodes(R"(
void main()
{
    Color=mtl.color;
})");
    }

    mc.CreateShader();

    return(true);
}

bool VertexColor2DMaterial()
{
    MaterialCreater mc(1,false);

    //vertex部分
    {
        ShaderCreaterVertex *vsc=mc.GetVS();

        vsc->AddInput("vec2","Position");
        vsc->AddInput("vec4","Color");

        vsc->AddOutput("vec4","Color");

        vsc->SetShaderCodes(R"(
void main()
{
    Output.Color=Color;

    gl_Position=vec4(Position,0,1);
})");
    }

    //fragment部分
    {
        ShaderCreaterFragment *fsc=mc.GetFS();

        fsc->AddOutput("vec4","Color");

        fsc->SetShaderCodes(R"(
void main()
{
    Color=Input.Color;
})");
    }

    mc.CreateShader();

    return(true);
}

namespace glsl_compiler
{
    bool Init();
    void Close();
}//namespace glsl_compiler

int main()
{
    if(!glsl_compiler::Init())
        return -1;

    PureColor2DMaterial();
    VertexColor2DMaterial();

    glsl_compiler::Close();

    return 0;
}
