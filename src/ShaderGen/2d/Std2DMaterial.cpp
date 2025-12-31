#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include"common/MFGetPosition.h"
#include"common/MFRectPrimitive.h"

STD_MTL_NAMESPACE_BEGIN
bool Std2DMaterial::CustomVertexShader(ShaderCreateInfoVertex *vsc)
{
    RANGE_CHECK_RETURN_FALSE(cfg->coordinate_system)

    vsc->AddInput(cfg->position_format,VAN::Position);

    const bool is_rect=(cfg->prim==PrimitiveType::SolidRectangles||cfg->prim==PrimitiveType::WireRectangles);
    
    if(cfg->local_to_world)
    {
        mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics,SBS_LocalToWorld);

        vsc->AddAssignTransform();
    }

    if(cfg->material_instance)
    {
        vsc->AddAssignMaterialInstance();
    }

    if(cfg->local_to_world)
    {
        mci->SetLocalToWorld((uint32_t)ShaderStage::AllGraphics);

        if(is_rect)
            vsc->AddFunction(func::GetPosition2DRectL2W[size_t(cfg->coordinate_system)]);
        else
            vsc->AddFunction(func::GetPosition2DL2W[size_t(cfg->coordinate_system)]);
    }
    else
    {
        if(is_rect)
            vsc->AddFunction(func::GetPosition2DRect[size_t(cfg->coordinate_system)]);
        else
            vsc->AddFunction(func::GetPosition2D[size_t(cfg->coordinate_system)]);
    }

    if(cfg->coordinate_system==CoordinateSystem2D::Ortho)
    {
        mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics,SBS_ViewportInfo);
    }

    return(true);
}
STD_MTL_NAMESPACE_END
