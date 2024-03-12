#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include"common/MFGetPosition.h"
#include"common/MFGetNormal.h"
#include"common/MFRectPrimitive.h"

STD_MTL_NAMESPACE_BEGIN
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

    if(cfg->camera
     &&cfg->local_to_world)
    {
        vsc->AddFunction(func::GetNormalMatrix);
        vsc->AddFunction(func::GetNormal);
//        vsc->AddFunction(func::GetNormalVS);
    }

    mci->AddStruct(SBS_ViewportInfo);

    mci->AddUBO(VK_SHADER_STAGE_ALL_GRAPHICS,
                DescriptorSetType::Static,
                SBS_ViewportInfo);

    return(true);
}
STD_MTL_NAMESPACE_END
