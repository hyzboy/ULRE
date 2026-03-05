#include"Std3DMaterial.h"

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateBillboard2DDynamic(const contract::PhysicalDeviceProfileLite *,mtl::BillboardMaterialCreateConfig *cfg);
MaterialCreateInfo *CreateBillboard2DFixedSize(const contract::PhysicalDeviceProfileLite *,mtl::BillboardMaterialCreateConfig *cfg);

MaterialCreateInfo *CreateBillboard2D(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(cfg->fixed_size)
        return CreateBillboard2DFixedSize(profile,cfg);
    else
        return CreateBillboard2DDynamic(profile,cfg);
}

}//namespace hgl::graph::mtl
