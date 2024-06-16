#include"Std3DMaterial.h"

STD_MTL_NAMESPACE_BEGIN

MaterialCreateInfo *CreateBillboard2DDynamic(mtl::BillboardMaterialCreateConfig *cfg);
MaterialCreateInfo *CreateBillboard2DFixedSize(mtl::BillboardMaterialCreateConfig *cfg);

MaterialCreateInfo *CreateBillboard2D(mtl::BillboardMaterialCreateConfig *cfg)
{
    if(cfg->fixed_size)
        return CreateBillboard2DFixedSize(cfg);
    else
        return CreateBillboard2DDynamic(cfg);
}

STD_MTL_NAMESPACE_END