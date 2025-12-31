#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/type/EnumUtil.h>

VK_NAMESPACE_BEGIN
void WriteDescriptorSetLayoutBinding(VkDescriptorSetLayoutBinding *dslb,ShaderDescriptor *sd)
{
    dslb->binding           =sd->binding;
    dslb->descriptorType    =sd->desc_type;
    dslb->descriptorCount   =1;
    dslb->stageFlags        =sd->stage_flag;
    dslb->pImmutableSamplers=nullptr;
}

MaterialDescriptorManager::MaterialDescriptorManager(const AnsiString &name,ShaderDescriptor *sd_list,const uint sd_count)
{
    mtl_name=name;

    if(sd_count<=0)return;

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        dsl_ci[i].bindingCount=0;
        dsl_ci[i].pBindings=nullptr;
    }

    {
        ShaderDescriptor *sp=sd_list;

        for(uint i=0;i<sd_count;i++)
        {
            binding_map[size_t(sp->set_type)][size_t(sp->desc_type)].Add(sp->name,sp->binding);

            ++dsl_ci[size_t(sp->set_type)].bindingCount;

            ++sp;
        }
    }

    all_dslb=new VkDescriptorSetLayoutBinding[sd_count];

    {
        VkDescriptorSetLayoutBinding *dsl_bind[DESCRIPTOR_SET_TYPE_COUNT];
        VkDescriptorSetLayoutBinding *dslp=all_dslb;

        ENUM_CLASS_FOR(DescriptorSetType,int,i)
            if(dsl_ci[i].bindingCount>0)
            {
                dsl_ci[i].pBindings=dslp;
                dsl_bind[i]=dslp;
                dslp+=dsl_ci[i].bindingCount;
            }

        {
            ShaderDescriptor *sp=sd_list;

            for(uint i=0;i<sd_count;i++)
            {
                WriteDescriptorSetLayoutBinding(dsl_bind[size_t(sp->set_type)],sp);

                ++dsl_bind[size_t(sp->set_type)];

                ++sp;                                
            }
        }
    }
}

MaterialDescriptorManager::MaterialDescriptorManager(const AnsiString &name,const ShaderDescriptorSetArray &sds_array)
{
    mtl_name=name;

    uint sd_count=0;

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        dsl_ci[i].bindingCount=sds_array[i].count;
        dsl_ci[i].pBindings=nullptr;

        sd_count+=sds_array[i].count;
    }

    if(sd_count<=0)
    {
        all_dslb=nullptr;
        return;
    }

    all_dslb=new VkDescriptorSetLayoutBinding[sd_count];

    {
        VkDescriptorSetLayoutBinding *dsl_bind[DESCRIPTOR_SET_TYPE_COUNT];
        VkDescriptorSetLayoutBinding *dslp=all_dslb;

        ENUM_CLASS_FOR(DescriptorSetType,int,i)
            if(dsl_ci[i].bindingCount>0)
            {
                dsl_ci[i].pBindings=dslp;
                dsl_bind[i]=dslp;
                dslp+=dsl_ci[i].bindingCount;
            }

        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            auto *sp=sds_array[i].descriptor_map.GetDataList();
            ShaderDescriptor *sd;

            for(int j=0;j<sds_array[i].count;j++)
            {
                sd=(*sp)->value;

                binding_map[size_t(sd->set_type)][size_t(sd->desc_type)].Add(sd->name,sd->binding);

                WriteDescriptorSetLayoutBinding(dsl_bind[i],sd);

                ++dsl_bind[i];

                ++sp;
            }
        }
    }
}

MaterialDescriptorManager::~MaterialDescriptorManager()
{
    delete[] all_dslb;
}
    
const int MaterialDescriptorManager::GetBinding(const DescriptorSetType &set_type,const VkDescriptorType &desc_type,const AnsiString &name)const
{
    RANGE_CHECK_RETURN(set_type,-1)

    if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
     ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE)
        return -1;

    if(name.IsEmpty())return -1;

    int result;

    return(binding_map[size_t(set_type)][size_t(desc_type)].Get(name,result)?result:-1);
}
VK_NAMESPACE_END
