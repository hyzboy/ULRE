#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/TypeFunc.h>

VK_NAMESPACE_BEGIN
void WriteDescriptorSetLayoutBinding(VkDescriptorSetLayoutBinding *dslb,ShaderDescriptor *sd)
{
    dslb->binding           =sd->binding;
    dslb->descriptorType    =sd->desc_type;
    dslb->descriptorCount   =1;
    dslb->stageFlags        =sd->stage_flag;
    dslb->pImmutableSamplers=nullptr;
}

MaterialDescriptorManager::MaterialDescriptorManager(const UTF8String &name,ShaderDescriptor *sd_list,const uint sd_count)
{
    mtl_name=name;

    if(sd_count<=0)return;

    ShaderDescriptorList sd_list_by_desc_type[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

    hgl_zero(set_has_desc);

    uint dslb_count=0;

    {
        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            dsl_ci[i].bindingCount=0;
            dsl_ci[i].pBindings=nullptr;
        }

        {
            ShaderDescriptor *sp=sd_list;

            for(uint i=0;i<sd_count;i++)
            {
                if(sp->desc_type>=VK_DESCRIPTOR_TYPE_BEGIN_RANGE
                 &&sp->desc_type<=VK_DESCRIPTOR_TYPE_END_RANGE)
                    sd_list_by_desc_type[(size_t)sp->desc_type].Add(sp);

                binding_map[size_t(sp->desc_type)].Add(sp->name,sp->binding);

                ++dsl_ci[size_t(sp->set_type)].bindingCount;

                ++dslb_count;

                set_has_desc[size_t(sp->set_type)]=true;

                ++sp;
            }
        }

        all_dslb=new VkDescriptorSetLayoutBinding[dslb_count];

        VkDescriptorSetLayoutBinding *dsl_bind[DESCRIPTOR_SET_TYPE_COUNT];
        VkDescriptorSetLayoutBinding *dslp=all_dslb;

        {
            ENUM_CLASS_FOR(DescriptorSetType,int,i)
                if(dsl_ci[i].bindingCount>0)
                {
                    dsl_ci[i].pBindings=dslp;
                    dsl_bind[i]=dslp;
                    dslp+=dsl_ci[i].bindingCount;
                }
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

MaterialDescriptorManager::~MaterialDescriptorManager()
{
    delete[] all_dslb;
}
    
const int MaterialDescriptorManager::GetBinding(const VkDescriptorType &desc_type,const AnsiString &name)const
{
    if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
     ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE)
        return -1;

    if(name.IsEmpty())return -1;

    int result;

    return(binding_map[size_t(desc_type)].Get(name,result)?result:-1);
}
VK_NAMESPACE_END