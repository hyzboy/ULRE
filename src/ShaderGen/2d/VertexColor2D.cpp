#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/2d/VertexColor2D.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
MaterialCreateInfo *CreateVertexColor2D(const CoordinateSystem2D &cs)
{
    AnsiString mtl_name="VertexColor2D";

    if(cs==CoordinateSystem2D::NDC)mtl_name+="NDC";else
    if(cs==CoordinateSystem2D::ZeroToOne)mtl_name+="ZeroToOne";else
    if(cs==CoordinateSystem2D::Ortho)mtl_name+="Ortho";else
        return(nullptr);


    MaterialCreateInfo *mci=new MaterialCreateInfo( mtl_name,               ///<名称
                                                    1,                      ///<最终一个RT输出
                                                    false);                 ///<无深度输出

    if(cs==CoordinateSystem2D::Ortho)
    {
        mci->AddStruct(GlobalShaderUBO::ViewportInfo,"");
    }

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
