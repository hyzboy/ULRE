#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/2d/VertexColor2D.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
MaterialCreateInfo *CreateVertexColor2D(const Material2DConfig *cfg)
{
    if(!cfg)return(nullptr);

    RANGE_CHECK_RETURN_NULLPTR(cfg->coordinate_system)

    AnsiString mtl_name;

    MaterialCreateInfo *mci=new MaterialCreateInfo( mtl_name,               ///<名称
                                                    1,                      ///<最终一个RT输出
                                                    false);                 ///<无深度输出

    AnsiString sfComputePosition;

    if(cfg->coordinate_system==CoordinateSystem2D::Ortho)
    {
        mtl_name="VertexColor2DOrtho";
    
        mci->AddUBO(VK_SHADER_STAGE_VERTEX_BIT,
                    DescriptorSetType::Global,
                    SBS_ViewportInfo);

        sfComputePosition="vec4 ComputePosition(vec4 pos){return viewport.ortho_matrix*pos;}";
    }
    else
    if(cfg->coordinate_system==CoordinateSystem2D::ZeroToOne)
    {
        mtl_name="VertexColor2DZeroToOne";

        sfComputePosition="vec4 ComputePosition(vec4 pos){return vec4(pos.xy*2-1,pos.z,pos.w);}";
    }
    else
    {
        mtl_name="VertexColor2DNDC";

        sfComputePosition="vec4 ComputePosition(vec4 pos){return pos;}";
    }

    //vertex部分
    {
        ShaderCreateInfoVertex *vsc=mci->GetVS();

        vsc->AddOutput(VAT_VEC4,"Color");

        vsc->AddInput(VAT_VEC2,VAN::Position);
        vsc->AddInput(VAT_VEC4,VAN::Color);

        vsc->AddFunction(sfComputePosition);

        AnsiString main_codes="void main()\n{\n\tOutput.Color=Color;\n";

        if(cfg->local_to_world)
        {
            vsc->AddLocalToWorld();
            
            vsc->AddFunction(mtl::func::GetLocalToWorld);

            main_codes+="\tvec4 pos=GetLocalToWorld()*vec4(Position,0,1);\n";
        }
        else
        {
            main_codes+="\tvec4 pos=vec4(Position,0,1);\n";
        }

        main_codes+="\tgl_Position=ComputePosition(pos);\n}";

        vsc->AddFunction(main_codes);
    }

    //fragment部分
    {
        ShaderCreateInfoFragment *fsc=mci->GetFS();

        fsc->AddOutput(VAT_VEC4,"Color");

        fsc->AddFunction(R"(
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
