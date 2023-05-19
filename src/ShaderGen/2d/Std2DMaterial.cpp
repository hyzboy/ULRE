#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/2d/Material2DConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include"common/MFGetPosition.h"
#include"common/MFCommon.h"

STD_MTL_NAMESPACE_BEGIN
Std2DMaterial::Std2DMaterial(const Material2DConfig *c)
{
    mci=new MaterialCreateInfo(c);

    cfg=c;
}

bool Std2DMaterial::CreateVertexShader(ShaderCreateInfoVertex *vsc)
{
    if(!RangeCheck(cfg->coordinate_system))
        return(false);

    vsc->AddInput(VAT_VEC2,VAN::Position);

    if(cfg->local_to_world)
    {
        vsc->AddLocalToWorld();
        vsc->AddFunction(func::GetLocalToWorld);

        vsc->AddFunction(func::GetPosition2DL2W[size_t(cfg->coordinate_system)]);
    }
    else
        vsc->AddFunction(func::GetPosition2D[size_t(cfg->coordinate_system)]);

    if(cfg->coordinate_system==CoordinateSystem2D::Ortho)
    {
        mci->AddUBO(VK_SHADER_STAGE_VERTEX_BIT,
                    DescriptorSetType::Global,
                    SBS_ViewportInfo);
    }

    return(true);
}

MaterialCreateInfo *Std2DMaterial::Create()
{
    if(mci->hasVertex())
        if(!CreateVertexShader(mci->GetVS()))
            return(nullptr);

    if(mci->hasGeometry())
        if(!CreateGeometryShader(mci->GetGS()))
            return(nullptr);

    if(mci->hasFragment())
        if(!CreateFragmentShader(mci->GetFS()))
            return(nullptr);

    if(!mci->CreateShader())
        return(nullptr);

    return(mci);
}
STD_MTL_NAMESPACE_END
