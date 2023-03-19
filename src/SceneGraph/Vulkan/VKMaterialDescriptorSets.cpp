#include<hgl/graph/VKMaterialDescriptorSets.h>
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

MaterialDescriptorSets::MaterialDescriptorSets(const UTF8String &name,ShaderDescriptor *sd_list,const uint sd_count)
{
    mtl_name=name;

    if(sd_count<=0)return;

    ShaderDescriptorList sd_list_by_desc_type[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

    hgl_zero(set_has_desc);

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

//                sd_by_name.Add(sp->name,sp);
                binding_map[size_t(sp->desc_type)].Add(sp->name,sp->binding);

                ++dsl_ci[size_t(sp->set_type)].bindingCount;

                //sd_list_by_set_type[size_t(sp->set_type)].Add(sp);
                set_has_desc[size_t(sp->set_type)]=true;

                ++sp;
            }
        }

        VkDescriptorSetLayoutBinding *dsl_bind[size_t(DescriptorSetType::RANGE_SIZE)];

        {
            ENUM_CLASS_FOR(DescriptorSetType,int,i)
                if(dsl_ci[i].bindingCount>0)
                {
                    dsl_ci[i].pBindings=new VkDescriptorSetLayoutBinding[dsl_ci[i].bindingCount];
                    dsl_bind[i]=(VkDescriptorSetLayoutBinding *)dsl_ci[i].pBindings;
                }
        }

        {
            ShaderDescriptorList *sdl=sd_list_by_desc_type;
            ShaderDescriptor **sdp;

            for(uint i=VK_DESCRIPTOR_TYPE_BEGIN_RANGE;
                     i<=VK_DESCRIPTOR_TYPE_END_RANGE;i++)
            {
                if(sdl->GetCount()>0)
                {
//                    binding_list[i]=new int[sdl->GetCount()];

                    sdp=sdl->GetData();
                    for(int j=0;j<sdl->GetCount();j++)
                    {
//                        binding_list[i][j]=(*sdp)->binding;

                        WriteDescriptorSetLayoutBinding(dsl_bind[size_t((*sdp)->set_type)],
                                                        *sdp);

                        ++dsl_bind[size_t((*sdp)->set_type)];

                        ++sdp;
                    }
                }
                //else
                //{
                //    binding_list[i]=nullptr;
                //}

                ++sdl;
            }
        }
    }
}

MaterialDescriptorSets::~MaterialDescriptorSets()
{
    ENUM_CLASS_FOR(DescriptorSetType,int,i)
        if(dsl_ci[i].bindingCount)
            delete[] dsl_ci[i].pBindings;

    //for(uint i=VK_DESCRIPTOR_TYPE_BEGIN_RANGE;
    //        i<=VK_DESCRIPTOR_TYPE_END_RANGE;i++)
    //{
    //    if(binding_list[i])
    //        delete[] binding_list[i];
    //}
}
    
const int MaterialDescriptorSets::GetBinding(const VkDescriptorType &desc_type,const AnsiString &name)const
{
    if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
     ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE)
        return -1;

    if(name.IsEmpty())return -1;

    int result;

    return(binding_map[size_t(desc_type)].Get(name,result)?result:-1);
}
VK_NAMESPACE_END