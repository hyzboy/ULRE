#include<hgl/shadergen/MaterialCreater.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::shadergen;

bool PureColorMaterial()
{
    MaterialCreater mc(1);                                  //一个新材质，1个RT输出，默认使用Vertex/Fragment shader

    //vertex部分
    {
        ShaderCreaterVertex *vsc=mc.GetVS();                //获取vertex shader creater

        //以下代码会被展开为
        /*
            layout(location=?) in vec3 Position;             //位置属性
        */
        vsc->AddInput("vec3","Position");                   //添加一个vec3类型的position属性输入

        //以下代码会被展开为
        /*
            vec3 GetPosition(void)
            {
                return Position;
            }
        */
        vsc->AddFunction("vec3","GetPosition","void","return Position;");

        //使用缺省main函数，会产生如下代码
        /*
            void main()
            {
                gl_Position=GetPosition();
            }
        */
        vsc->UseDefaultMain();
    }

    //添加一个名称为ColorMaterial的UBO定义,其内部有一个vec4 color的属性
    //该代码会被展开为
    /*
        struct ColorMaterial
        {
            vec4 color;
        };
     */
    mc.AddUBOStruct("ColorMaterial","vec4 color;");

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

        //以下代码会被合并成 vec4 GetColor(/**/){return mtl.color;}
        fsc->AddFunction("vec4","GetColor","/**/","return mtl.color;");

        //以下代码会被展开为
        /*
            layout(location=?) out vec4 Color;              //颜色输出
        */
        fsc->AddOutput("vec4","Color");                     //添加一个vec4类型的color属性输出
        //同时，如果拥有多个输出，会直接根据输出的数量，DefaultMain会根据对应的Output名称，要求GetXXX()函数

        //使用缺省main函数，会产生如下代码    
        /*
            void main()
            {
                Color=GetColor();
            }
        */
        fsc->UseDefaultMain();
    }

    return(false);
}

int MaterialCreaterTest()
{
        

    return 0;
}
