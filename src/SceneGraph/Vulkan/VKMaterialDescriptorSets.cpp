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

MaterialDescriptorSets::MaterialDescriptorSets(ShaderDescriptor *sd,const uint count)
{
    sd_list=sd;
    sd_count=count;

    if(sd_count<=0)return;

    {
        hgl_zero(sds);

        {
            ShaderDescriptor *sp=sd_list;

            for(uint i=0;i<sd_count;i++)
            {
                if(sp->desc_type>=VK_DESCRIPTOR_TYPE_BEGIN_RANGE
                 &&sp->desc_type<=VK_DESCRIPTOR_TYPE_END_RANGE)
                    descriptor_list[(size_t)sp->desc_type].Add(sp);

                sd_by_name.Add(sp->name,sp);
                binding_map[size_t(sp->desc_type)].Add(sp->name,sp->binding);

                ++sds[sp->set].count;

                ++sp;
            }
        }

        VkDescriptorSetLayoutBinding *sds_ptr[size_t(DescriptorSetType::RANGE_SIZE)];

        {
            ENUM_CLASS_FOR(DescriptorSetType,int,i)
                if(sds[i].count>0)
                {
                    sds[i].binding_list=new VkDescriptorSetLayoutBinding[sds[i].count];
                    sds_ptr[i]=sds[i].binding_list;
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

                        WriteDescriptorSetLayoutBinding(sds_ptr[(*sdp)->set],
                                                        *sdp);

                        ++sds_ptr[(*sdp)->set];

                        ++sdp;
                    }
                }
                else
                {
                    binding_list[i]=nullptr;
                }
            }
        }
    }
}

MaterialDescriptorSets::~MaterialDescriptorSets()
{
    ENUM_CLASS_FOR(DescriptorSetType,int,i)
        if(sds[i].count)
            delete[] sds[i].binding_list;

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