#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/type/EnumUtil.h>
#include<vector>
#include<algorithm>

namespace hgl::graph{
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

    std::vector<ShaderDescriptor*> set_values[DESCRIPTOR_SET_TYPE_COUNT];

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        sds_array[i].descriptor_map.GetValueArray(set_values[i]);

        set_values[i].erase(
            std::remove(set_values[i].begin(),set_values[i].end(),nullptr),
            set_values[i].end());

        dsl_ci[i].bindingCount=static_cast<uint32_t>(set_values[i].size());
        dsl_ci[i].pBindings=nullptr;

        sd_count+=dsl_ci[i].bindingCount;
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
            auto &values=set_values[i];

            std::sort(values.begin(),values.end(),
                [](const ShaderDescriptor *lhs,const ShaderDescriptor *rhs)
                {
                    if(!lhs||!rhs)
                        return lhs<rhs;

                    return lhs->binding<rhs->binding;
                });

            for(auto sd:values)
            {
                binding_map[size_t(sd->set_type)][size_t(sd->desc_type)].Add(sd->name,sd->binding);

                WriteDescriptorSetLayoutBinding(dsl_bind[i],sd);

                ++dsl_bind[i];
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
}//namespace hgl::graph
