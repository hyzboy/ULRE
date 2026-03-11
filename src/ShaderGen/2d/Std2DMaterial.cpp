#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/UBOCommon.h>
#include"common/MFGetPosition.h"

namespace hgl::graph::mtl{
bool Std2DMaterial::CustomVertexShader(ShaderCreateInfoVertex *vsc)
{
    RANGE_CHECK_RETURN_FALSE(cfg->coordinate_system)

    vsc->AddInput(cfg->position_format,VAN::Position);

    if(cfg->local_to_world)
    {
        vsc->AddAssignTransform();
    }

    if(cfg->material_instance)
    {
        vsc->AddAssignMaterialInstance();
    }

    if(cfg->local_to_world)
    {
        mci->SetLocalToWorld((uint32_t)ShaderStage::AllGraphics);

        vsc->AddFunction(func::GetPosition2DL2W[size_t(cfg->coordinate_system)]);
    }
    else
    {
        vsc->AddFunction(func::GetPosition2D[size_t(cfg->coordinate_system)]);
    }

    if(cfg->coordinate_system==CoordinateSystem2D::Ortho)
    {
        mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics,SBS_ViewportInfo);
    }

    return(true);
}
}//namespace hgl::graph::mtl
