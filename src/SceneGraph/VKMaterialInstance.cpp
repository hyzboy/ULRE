#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialInstance.h>

namespace hgl::graph{
MaterialInstance *Material::CreateMI(const VIL *vil)
{
    // In the new path Material no longer owns a data store.
    // mi_id is always -1; data lives in an external SSBO managed by the caller.
    return new MaterialInstance(this, vil ? vil : GetDefaultVIL(), -1);
}

MaterialInstance *Material::CreateMI(const VILConfig *vil_cfg)
{
    return CreateMI(CreateVIL(vil_cfg));
}

MaterialInstance::MaterialInstance(Material *mtl,const VIL *v,const int id)
{
    material=mtl;
    vil=v;
    mi_id=id;
}
}//namespace hgl::graph
