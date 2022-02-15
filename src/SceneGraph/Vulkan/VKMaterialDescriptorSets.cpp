#include<hgl/graph/VKMaterialDescriptorSets.h>
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

MaterialDescriptorSets::MaterialDescriptorSets(const UTF8String &name,ShaderDescriptor *sd,const uint count)
{
    mtl_name=name;

    sd_list=sd;
    sd_count=count;

    if(sd_count<=0)return;

    {
        ENUM_CLASS_FOR(DescriptorSetsType,int,i)
        {
            sds[i].bindingCount=0;
            sds[i].pBindings=nullptr;
        }

        {
            ShaderDescriptor *sp=sd_list;

            for(uint i=0;i<sd_count;i++)
            {
                if(sp->desc_type>=VK_DESCRIPTOR_TYPE_BEGIN_RANGE
                 &&sp->desc_type<=VK_DESCRIPTOR_TYPE_END_RANGE)
                    descriptor_list[(size_t)sp->desc_type].Add(sp);

                sd_by_name.Add(sp->name,sp);
                binding_map[size_t(sp->desc_type)].Add(sp->name,sp->binding);

                ++sds[size_t(sp->set_type)].bindingCount;

                ++sp;
            }
        }

        VkDescriptorSetLayoutBinding *sds_ptr[size_t(DescriptorSetsType::RANGE_SIZE)];

        {
            ENUM_CLASS_FOR(DescriptorSetsType,int,i)
                if(sds[i].bindingCount>0)
                {
                    sds[i].pBindings=new VkDescriptorSetLayoutBinding[sds[i].bindingCount];
                    sds_ptr[i]=(VkDescriptorSetLayoutBinding *)sds[i].pBindings;
                }
        }

        {
            ShaderDescriptorList *sdl=descriptor_list;
            ShaderDescriptor **sdp;

            for(uint i=VK_DESCRIPTOR_TYPE_BEGIN_RANGE;
                     i<=VK_DESCRIPTOR_TYPE_END_RANGE;i++)
            {
                if(sdl->GetCount()>0)
                {
                    binding_list[i]=new int[sdl->GetCount()];

                    sdp=sdl->GetData();
                    for(int j=0;j<sdl->GetCount();j++)
                    {
                        binding_list[i][j]=(*sdp)->binding;

                        WriteDescriptorSetLayoutBinding(sds_ptr[size_t((*sdp)->set_type)],
                                                        *sdp);

                        ++sds_ptr[size_t((*sdp)->set_type)];

                        ++sdp;
                    }
                }
                else
                {
                    binding_list[i]=nullptr;
                }

                ++sdl;
            }
        }
    }
}

MaterialDescriptorSets::~MaterialDescriptorSets()
{
    ENUM_CLASS_FOR(DescriptorSetsType,int,i)
        if(sds[i].bindingCount)
            delete[] sds[i].pBindings;

    delete[] sd_list;       //"delete[] nullptr" isn't bug.
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