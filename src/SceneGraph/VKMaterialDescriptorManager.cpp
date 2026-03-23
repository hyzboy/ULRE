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

void MaterialDescriptorManager::InitEnumBindingMaps()
{
    for(int st=0;st<DESCRIPTOR_SET_TYPE_COUNT;++st)
    {
        for(size_t i=0;i<mtl::UBODescriptorSemanticCount;++i)
            for(int d=0;d<2;++d)
                ubo_binding_map[st][i][d]=-1;

        for(size_t i=0;i<mtl::SSBODescriptorSemanticCount;++i)
            for(int d=0;d<2;++d)
                ssbo_binding_map[st][i][d]=-1;

        for(size_t i=0;i<mtl::SamplerSlotCount;++i)
        {
            texture_binding_map[st][i]=-1;
            texture_sampler_binding_map[st][i]=-1;
        }
    }
}

void MaterialDescriptorManager::RegisterEnumBinding(const ShaderDescriptor *sd)
{
    if(!sd)
        return;

    const size_t st=size_t(sd->set_type);

    switch(sd->desc_type)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        {
            if(!mtl::IsBuiltinDescriptorSemantic(sd->semantic))
                break;

            const mtl::UBODescriptorSemantic semantic=mtl::ToUBODescriptorSemantic(sd->semantic);
            const size_t semantic_index=size_t(semantic);
            if(semantic==mtl::UBODescriptorSemantic::Unknown
            || semantic_index>=mtl::UBODescriptorSemanticCount)
                break;

            const int dynamic=(sd->desc_type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)?1:0;
            ubo_binding_map[st][semantic_index][dynamic]=sd->binding;
        }
        break;

        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        {
            if(!mtl::IsBuiltinDescriptorSemantic(sd->semantic))
                break;

            const mtl::SSBODescriptorSemantic semantic=mtl::ToSSBODescriptorSemantic(sd->semantic);
            const size_t semantic_index=size_t(semantic);
            if(semantic==mtl::SSBODescriptorSemantic::Unknown
            || semantic_index>=mtl::SSBODescriptorSemanticCount)
                break;

            const int dynamic=(sd->desc_type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)?1:0;
            ssbo_binding_map[st][semantic_index][dynamic]=sd->binding;
        }
        break;

        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        {
            mtl::SamplerSlot slot=mtl::SamplerSlot::BaseColor;
            if(mtl::TryGetSlotFromDescriptorName(sd->name,slot))
            {
                const size_t slot_index=size_t(slot);
                if(slot_index<mtl::SamplerSlotCount)
                    texture_binding_map[st][slot_index]=sd->binding;
            }
        }
        break;

        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        {
            mtl::SamplerSlot slot=mtl::SamplerSlot::BaseColor;
            if(mtl::TryGetSlotFromDescriptorName(sd->name,slot))
            {
                const size_t slot_index=size_t(slot);
                if(slot_index<mtl::SamplerSlotCount)
                    texture_sampler_binding_map[st][slot_index]=sd->binding;
            }
        }
        break;

        default:
        break;
    }
}

MaterialDescriptorManager::MaterialDescriptorManager(const AnsiString &name,ShaderDescriptor *sd_list,const uint sd_count)
{
    mtl_name=name;

    InitEnumBindingMaps();

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
            RegisterEnumBinding(sp);

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

    InitEnumBindingMaps();

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
                RegisterEnumBinding(sd);

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

const int MaterialDescriptorManager::GetUBO(const DescriptorSetType &set_type,const mtl::UBODescriptorSemantic semantic,bool dynamic)const
{
    RANGE_CHECK_RETURN(set_type,-1)

    const size_t semantic_index=size_t(semantic);
    if(semantic_index>=mtl::UBODescriptorSemanticCount)
        return -1;

    return ubo_binding_map[size_t(set_type)][semantic_index][dynamic?1:0];
}

const int MaterialDescriptorManager::GetSSBO(const DescriptorSetType &set_type,const mtl::SSBODescriptorSemantic semantic,bool dynamic)const
{
    RANGE_CHECK_RETURN(set_type,-1)

    const size_t semantic_index=size_t(semantic);
    if(semantic_index>=mtl::SSBODescriptorSemanticCount)
        return -1;

    return ssbo_binding_map[size_t(set_type)][semantic_index][dynamic?1:0];
}

const int MaterialDescriptorManager::GetTexture(const DescriptorSetType &set_type,const mtl::SamplerSlot slot)const
{
    RANGE_CHECK_RETURN(set_type,-1)

    const size_t slot_index=size_t(slot);
    if(slot_index>=mtl::SamplerSlotCount)
        return -1;

    return texture_binding_map[size_t(set_type)][slot_index];
}

const int MaterialDescriptorManager::GetTextureSampler(const DescriptorSetType &set_type,const mtl::SamplerSlot slot)const
{
    RANGE_CHECK_RETURN(set_type,-1)

    const size_t slot_index=size_t(slot);
    if(slot_index>=mtl::SamplerSlotCount)
        return -1;

    return texture_sampler_binding_map[size_t(set_type)][slot_index];
}
}//namespace hgl::graph
