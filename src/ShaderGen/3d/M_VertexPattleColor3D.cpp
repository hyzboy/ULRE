/** 顶点调色板色要求有一个UBO结构如下
*
*
*   struct ColorPattle
*   {
*       vec4 color[256];
*   }color_pattle;
*
*   然后输入的一个R8UI顶点属性来指定使用那个颜色。
*/ 

#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/UBOCommon.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.Color=color_pattle.color[Color.r];  //Color是输入的R8UI顶点属性

    gl_Position=GetPosition3D();
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=Input.Color;
})";// ^       ^
    // |       |
    // |       +--ps:这里的Input.Color就是上一个Shader中的Output.Color
    // +--ps:这里的Color就是最终的RT

    class MaterialVertexPattleColor3D:public Std3DMaterial
    {
    public:

        using Std3DMaterial::Std3DMaterial;
        ~MaterialVertexPattleColor3D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

            mci->AddUBOStruct((uint32_t)ShaderStage::Vertex,SBS_ColorPattle);


            vsc->AddInput(VAT_UINT,"Color",VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::Basic); //输入的R8UI顶点属性

            vsc->AddOutput(SVT_VEC4,"Color");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialVertexPattleColor3D:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateVertexPattleColor3D(const VulkanDevAttr *dev_attr,const Material3DCreateConfig *cfg)
{
    MaterialVertexPattleColor3D mvc3d(cfg);

    return mvc3d.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
