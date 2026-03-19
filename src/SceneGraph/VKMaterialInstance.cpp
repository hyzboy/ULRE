#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKResourceDomain.h>

namespace hgl::graph{

// ---------------------------------------------------------------------------
// Material::CreateMI — 旧路径（不涉及 ResourceDomain）
// ---------------------------------------------------------------------------

MaterialInstance *Material::CreateMI(const VIL *vil)
{
    // Phase 5: 旧路径统一通过懒初始化的 default_domain 分配 MI 槽位
    if(!default_domain && hasMI())
        default_domain = new ResourceDomain(this);

    int mi_id = default_domain ? default_domain->AllocMISlot() : -1;

    return new MaterialInstance(this, default_domain, vil ? vil : GetDefaultVIL(), mi_id);
}

MaterialInstance *Material::CreateMI(const VILConfig *vil_cfg)
{
    return CreateMI(CreateVIL(vil_cfg));
}

void Material::ReleaseMI(int mi_id)
{
    // Phase 5: 保留接口兼容性；通过 default_domain 代理释放
    if(mi_id < 0 || !default_domain) return;

    default_domain->FreeMISlot(mi_id);
}

void *Material::GetMIData(int id)
{
    // Phase 5: 通过 default_domain 代理访问
    if(!default_domain)
        return nullptr;

    return default_domain->GetMIData(id);
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

