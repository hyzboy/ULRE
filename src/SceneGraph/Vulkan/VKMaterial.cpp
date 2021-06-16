#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include"VKDescriptorSetLayoutCreater.h"
VK_NAMESPACE_BEGIN
Material::Material(const UTF8String &name,ShaderModuleMap *smm,List<VkPipelineShaderStageCreateInfo> *psci_list,DescriptorSetLayoutCreater *dslc)
{
    mtl_name=name;
    shader_maps=smm;
    shader_stage_list=psci_list;
    dsl_creater=dslc;

    const ShaderModule *sm;
    if(smm->Get(VK_SHADER_STAGE_VERTEX_BIT,sm))
    {
        vertex_sm=(VertexShaderModule *)sm;
        vab=vertex_sm->CreateVertexAttributeBinding();
    }
    else
    {
        //理论上不可能到达这里，前面CreateMaterial已经检测过了
        vertex_sm=nullptr;
        vab=nullptr;
    }

    mp_m=CreateMP(DescriptorSetsType::Material);
    mp_r=CreateMP(DescriptorSetsType::Renderable);
    mp_g=CreateMP(DescriptorSetsType::Global);
}

Material::~Material()
{
    SAFE_CLEAR(mp_m);
    SAFE_CLEAR(mp_r);
    SAFE_CLEAR(mp_g);

    delete dsl_creater;

    if(vab)
    {
        vertex_sm->Release(vab);
        delete vab;
    }

    delete shader_stage_list;
    delete shader_maps;
}

const int Material::GetBinding(VkDescriptorType desc_type,const AnsiString &name)const
{
    if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
     ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE
     ||name.IsEmpty())
        return(-1);

    int binding;
    const int shader_count=shader_maps->GetCount();

    const ShaderModule *sm;
    auto **itp=shader_maps->GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;
        binding=sm->GetBinding(desc_type,name);
        if(binding!=-1)
            return binding;

        ++itp;
    }

    return(-1);
}

const VkPipelineLayout Material::GetPipelineLayout()const
{
    return dsl_creater->GetPipelineLayout();
}

MaterialParameters *Material::CreateMP(const DescriptorSetsType &type)
{
    DescriptorSets *ds=dsl_creater->Create(type);

    if(!ds)return(nullptr);

    return(new MaterialParameters(this,type,ds));
}
VK_NAMESPACE_END
