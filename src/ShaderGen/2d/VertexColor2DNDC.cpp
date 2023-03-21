#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
MaterialCreateInfo *CreateVertexColor2DNDC()
{
    MaterialCreateInfo *mci=new MaterialCreateInfo("VertexColor2DNDC",      ///<名称，随便起
                                                    1,                      ///<最终一个RT输出
                                                    false);                 ///<无深度输出

    //vertex部分
    {
        ShaderCreateInfoVertex *vsc=mci->GetVS();

        vsc->AddInput(VAT_VEC2,VAN::Position);
        vsc->AddInput(VAT_VEC4,VAN::Color);

        vsc->AddOutput(VAT_VEC4,"Color");

        vsc->SetShaderCodes(R"(
void main()
{
    Output.Color=Color;

    gl_Position=vec4(Position,0,1);
})");
    }

    //fragment部分
    {
        ShaderCreateInfoFragment *fsc=mci->GetFS();

        fsc->AddOutput(VAT_VEC4,"Color");

        fsc->SetShaderCodes(R"(
void main()
{
    Color=Input.Color;
})");
    }

    if(mci->CreateShader())
        return mci;

    delete mci;
    return(nullptr);
}
STD_MTL_NAMESPACE_END
