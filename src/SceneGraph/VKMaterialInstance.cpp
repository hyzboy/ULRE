#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKResourceDomain.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

namespace hgl::graph{

// ---------------------------------------------------------------------------
// Material::CreateMI — 旧路径（不涉及 ResourceDomain）
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// MaterialInstance — constructors / destructor
// ---------------------------------------------------------------------------

/// 旧路径：domain = nullptr
MaterialInstance::MaterialInstance(Material *mtl, const VIL *v, const int id)
    : material(mtl), domain(nullptr), vil(v), mi_id(id)
{}

/// Phase 1 新路径：经由 ResourceDomain 分配
MaterialInstance::MaterialInstance(Material *mtl, ResourceDomain *d, const VIL *v, const int id)
    : material(mtl), domain(d), vil(v), mi_id(id)
{}

MaterialInstance::~MaterialInstance()
{
    if(domain)
        domain->FreeMISlot(mi_id);
    else
        material->ReleaseMI(mi_id);
}

// ---------------------------------------------------------------------------
// MaterialInstance — data access
// ---------------------------------------------------------------------------

void *MaterialInstance::GetMIData()
{
    if(domain)
        return domain->GetMIData(mi_id);

    return material->GetMIData(mi_id);
}

void MaterialInstance::WriteMIData(const void *data,const uint32 size)
{
    if(!data||!size||size>material->GetMIDataBytes())return;

    void *tp=GetMIData();

    if(tp)
        memcpy(tp,data,size);
}

}//namespace hgl::graph

