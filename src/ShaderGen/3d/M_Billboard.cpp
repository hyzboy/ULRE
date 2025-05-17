#include"Std3DMaterial.h"

STD_MTL_NAMESPACE_BEGIN

MaterialCreateInfo *CreateBillboard2DDynamic(const VulkanDevAttr *,mtl::BillboardMaterialCreateConfig *cfg);
MaterialCreateInfo *CreateBillboard2DFixedSize(const VulkanDevAttr *,mtl::BillboardMaterialCreateConfig *cfg);

MaterialCreateInfo *CreateBillboard2D(const VulkanDevAttr *dev_attr,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(cfg->fixed_size)
        return CreateBillboard2DFixedSize(dev_attr,cfg);
    else
        return CreateBillboard2DDynamic(dev_attr,cfg);
}

STD_MTL_NAMESPACE_END
