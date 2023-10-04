#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/3d/Material3DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include"common/MFGetPosition.h"
#include"common/MFGetNormal.h"
#include"common/MFRectPrimitive.h"

STD_MTL_NAMESPACE_BEGIN
Std3DMaterial::Std3DMaterial(const Material3DCreateConfig *c)
{
    mci=new MaterialCreateInfo(c);

    cfg=c;
}

bool Std3DMaterial::CustomVertexShader(ShaderCreateInfoVertex *vsc)
{
    vsc->AddInput(cfg->position_format,VAN::Position);

    if(cfg->camera)
    {
        mci->AddStruct(SBS_CameraInfo);

        mci->AddUBO(VK_SHADER_STAGE_ALL_GRAPHICS,
                    DescriptorSetType::Global,
                    SBS_CameraInfo);
    }

    if(cfg->local_to_world)
    {
        mci->SetLocalToWorld(VK_SHADER_STAGE_ALL_GRAPHICS);

        vsc->AddAssign();
        vsc->AddFunction(cfg->camera?func::GetPosition3DL2WCamera:func::GetPosition3DL2W);
    }
    else
        vsc->AddFunction(cfg->camera?func::GetPosition3DCamera:func::GetPosition3D);

    //if(cfg->camera
    // &&cfg->local_to_world)
    //{
    //    vsc->AddFunction(func::GetNormalMatrix);
    //    vsc->AddFunction(func::GetNormal);
    //}

    mci->AddUBO(VK_SHADER_STAGE_VERTEX_BIT,
                DescriptorSetType::Global,
                SBS_ViewportInfo);

    return(true);
}

MaterialCreateInfo *Std3DMaterial::Create()
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

    if(!mci->CreateShader())
        return(nullptr);

    return(mci);
}
STD_MTL_NAMESPACE_END
