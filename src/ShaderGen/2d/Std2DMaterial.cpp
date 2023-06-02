#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/2d/Material2DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include"common/MFGetPosition.h"

STD_MTL_NAMESPACE_BEGIN
Std2DMaterial::Std2DMaterial(const Material2DCreateConfig *c)
{
    mci=new MaterialCreateInfo(c);

    cfg=c;
}

bool Std2DMaterial::CustomVertexShader(ShaderCreateInfoVertex *vsc)
{
    RANGE_CHECK_RETURN_FALSE(cfg->coordinate_system)

    vsc->AddInput(VAT_VEC2,VAN::Position);

    if(cfg->local_to_world)
    {
        vsc->AddLocalToWorld();

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
    if(!BeginCustomShader())
        return(nullptr);

    if(mci->hasVertex())
        if(!CustomVertexShader(mci->GetVS()))
            return(nullptr);

    if(mci->hasGeometry())
        if(!CustomGeometryShader(mci->GetGS()))
            return(nullptr);

    if(mci->hasFragment())
        if(!CustomFragmentShader(mci->GetFS()))
            return(nullptr);

    if(!EndCustomShader())
        return(false);

    if(!mci->CreateShader(cfg->dev_attr))
        return(nullptr);

    return(mci);
}
STD_MTL_NAMESPACE_END
