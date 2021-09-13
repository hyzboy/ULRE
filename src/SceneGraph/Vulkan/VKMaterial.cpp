#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
#include"VKDescriptorSetLayoutCreater.h"
VK_NAMESPACE_BEGIN
Material::Material(const UTF8String &name,ShaderModuleMap *smm,MaterialDescriptorSets *_mds,DescriptorSetLayoutCreater *dslc)
{
    mtl_name=name;
    shader_maps=smm;
    mds=_mds;
    dsl_creater=dslc;

    const ShaderModule *sm;

    {
        const int shader_count=shader_maps->GetCount();
        shader_stage_list.SetCount(shader_count);
    
        VkPipelineShaderStageCreateInfo *p=shader_stage_list.GetData();        

        auto **itp=shader_maps->GetDataList();
        for(int i=0;i<shader_count;i++)
        {
            sm=(*itp)->right;
            hgl_cpy(p,sm->GetCreateInfo());

            ++p;
            ++itp;
        }
    }

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

    mp_m=CreateMP(DescriptorSetType::Material);
    mp_r=CreateMP(DescriptorSetType::Renderable);
    mp_g=CreateMP(DescriptorSetType::Global);
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

    delete shader_maps;
    SAFE_CLEAR(mds);
}

const VkPipelineLayout Material::GetPipelineLayout()const
{
    return dsl_creater->GetPipelineLayout();
}

MaterialParameters *Material::CreateMP(const DescriptorSetType &type)const
{
    DescriptorSets *ds=dsl_creater->Create(type);

    if(!ds)return(nullptr);

    return(new MaterialParameters(mds,type,ds));
}
VK_NAMESPACE_END
