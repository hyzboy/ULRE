#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

VK_NAMESPACE_BEGIN
MaterialInstance *Material::CreateMI(const VILConfig *vil_cfg)
{
    VIL *vil=CreateVIL(vil_cfg);

    if(!vil)return(nullptr);

    int mi_id=-1;

    if(mi_data_manager)
        mi_data_manager->GetOrCreate(&mi_id,1);
    else 
        mi_id=-1;

    return(new MaterialInstance(this,vil,mi_id));
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
 
void MaterialInstance::WriteMIData(const void *data,const int size)
{
    if(!data||size<=0||size>material->GetMIDataBytes())return;

    void *tp=GetMIData();

    if(tp)
        memcpy(tp,data,size);
}

MaterialInstance::MaterialInstance(Material *mtl,VIL *v,const int id)
{
    material=mtl;

    vil=v;

    mi_id=id;
}

bool MaterialInstance::BindUBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp=material->GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindUBO(name,ubo,dynamic))return(false);

    mp->Update();
    return(true);
}

bool MaterialInstance::BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp=material->GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindSSBO(name,ubo,dynamic))return(false);

    mp->Update();
    return(true);
}

bool MaterialInstance::BindImageSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=material->GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindImageSampler(name,tex,sampler))return(false);

    mp->Update();
    return(true);
}
VK_NAMESPACE_END
