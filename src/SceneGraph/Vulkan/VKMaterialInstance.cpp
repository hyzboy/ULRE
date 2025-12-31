#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

VK_NAMESPACE_BEGIN
MaterialInstance *Material::CreateMI(const VIL *vil)
{
    int mi_id=-1;

    if(mi_data_manager)
        mi_data_manager->GetOrCreate(&mi_id,1);
    else 
        mi_id=-1;

    return(new MaterialInstance(this,vil?vil:GetDefaultVIL(),mi_id));
}

MaterialInstance *Material::CreateMI(const VILConfig *vil_cfg)
{
    return CreateMI(CreateVIL(vil_cfg));
}

void Material::ReleaseMI(int mi_id)
{
    if(mi_id<0||!mi_data_manager)return;

    mi_data_manager->Release(&mi_id,1);
}

void *Material::GetMIData(int id)
{
    if(!mi_data_manager)
        return(nullptr);

    return mi_data_manager->GetData(id);
}
 
void MaterialInstance::WriteMIData(const void *data,const uint32 size)
{
    if(!data||!size||size>material->GetMIDataBytes())return;

    void *tp=GetMIData();

    if(tp)
        memcpy(tp,data,size);
}

MaterialInstance::MaterialInstance(Material *mtl,const VIL *v,const int id)
{
    material=mtl;

    vil=v;

    mi_id=id;
}
VK_NAMESPACE_END
