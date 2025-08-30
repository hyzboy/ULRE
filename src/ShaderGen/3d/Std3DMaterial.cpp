#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include"common/MFGetPosition.h"
#include"common/MFGetNormal.h"

STD_MTL_NAMESPACE_BEGIN
bool Std3DMaterial::CustomVertexShader(ShaderCreateInfoVertex *vsc)
{
    vsc->AddInput(cfg->position_format,VAN::Position);
    
    if(cfg->camera||cfg->local_to_world||cfg->material_instance)
    {
        mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics,SBS_LocalToWorld);

        vsc->AddAssign();
    }

    if(cfg->camera)
    {
        mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics,SBS_CameraInfo);
    }

    if(cfg->sky)
    {
        mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics,SBS_SkyInfo);
    }

    if(cfg->local_to_world)
    {
        mci->SetLocalToWorld((uint32_t)ShaderStage::AllGraphics);

        if(cfg->position_format.vec_size==3)
            vsc->AddFunction(cfg->camera?func::GetPosition3DL2WCamera:func::GetPosition3DL2W);
        else
        if(cfg->position_format.vec_size==2)
            vsc->AddFunction(cfg->camera?func::GetPosition3DL2WCameraBy2D:func::GetPosition3DL2WBy2D);
    }
    else
    {
        if(cfg->position_format.vec_size==3)
            vsc->AddFunction(cfg->camera?func::GetPosition3DCamera:func::GetPosition3D);
        else
        if(cfg->position_format.vec_size==2)
            vsc->AddFunction(cfg->camera?func::GetPosition3DCameraBy2D:func::GetPosition3DBy2D);
    }

    if(cfg->camera
     &&cfg->local_to_world)
    {
        vsc->AddFunction(func::GetNormalMatrix);
        vsc->AddFunction(func::GetNormal);

        if(vsc->hasInput(VAN::Normal))
        vsc->AddFunction(func::GetNormalVS);
    }

    mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics,SBS_ViewportInfo);

    return(true);
}
STD_MTL_NAMESPACE_END
