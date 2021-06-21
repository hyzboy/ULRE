#include"VKDescriptorSetLayoutCreater.h"
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
DescriptorSetLayoutCreater *GPUDevice::CreateDescriptorSetLayoutCreater()
{
    return(new DescriptorSetLayoutCreater(attr->device,attr->desc_pool));
}

DescriptorSetLayoutCreater::~DescriptorSetLayoutCreater()
{
    if(pipeline_layout)
        vkDestroyPipelineLayout(device,pipeline_layout,nullptr);

    for(ShaderDescriptorSet s:sds)
        if(s.layout)
            vkDestroyDescriptorSetLayout(device,s.layout,nullptr);
}

void DescriptorSetLayoutCreater::Bind(const ShaderDescriptorList *sd_list,VkDescriptorType desc_type,VkShaderStageFlagBits stageFlags)
{
    if(!sd_list||sd_list->GetCount()<=0)return;

    uint32_t binding_count[size_t(DescriptorSetsType::RANGE_SIZE)],
             old_count[size_t(DescriptorSetsType::RANGE_SIZE)],
             fin_count[size_t(DescriptorSetsType::RANGE_SIZE)];
    VkDescriptorSetLayoutBinding *p[size_t(DescriptorSetsType::RANGE_SIZE)];

    hgl_zero(binding_count);
    hgl_zero(old_count);
    hgl_zero(fin_count);
    hgl_zero(p);

    for(const ShaderDescriptor &sd:*sd_list)
    {
        all_set.Add(sd.set);

        ++binding_count[sd.set];
    }

    ENUM_CLASS_FOR(DescriptorSetsType,int,i)
        if(binding_count[i]>0)
        {
            old_count[i]=sds[i].binding_list.GetCount();

            sds[i].binding_list.PreMalloc(old_count[i]+binding_count[i]);
            p[i]=sds[i].binding_list.GetData()+old_count[i];
        }

    for(const ShaderDescriptor &sd:*sd_list)
    {
        //重复的绑定点是有可能存在的，比如CameraInfo在vs/fs中同时存在

        if(!all_binding.IsMember(sd.binding))
        {            
            p[sd.set]->binding              = sd.binding;
            p[sd.set]->descriptorType       = desc_type;
            p[sd.set]->descriptorCount      = 1;
            p[sd.set]->stageFlags           = stageFlags;
            p[sd.set]->pImmutableSamplers   = nullptr;

            all_binding.Add(sd.binding);

            ++p[sd.set];
            ++fin_count[sd.set];
        }
    }
    
    ENUM_CLASS_FOR(DescriptorSetsType,int,i)
        if(binding_count[i]>0)
            sds[i].binding_list.SetCount(old_count[i]+fin_count[i]);
}

bool DescriptorSetLayoutCreater::CreatePipelineLayout()
{
    fin_dsl_count=0;

    ENUM_CLASS_FOR(DescriptorSetsType,int,i)
    {
        const int count=sds[i].binding_list.GetCount();

        if(count<=0)
            continue;

        DescriptorSetLayoutCreateInfo descriptor_layout;

        descriptor_layout.bindingCount  = count;
        descriptor_layout.pBindings     = sds[i].binding_list.GetData();

        if(sds[i].layout)
            vkDestroyDescriptorSetLayout(device,sds[i].layout,nullptr);

        if(vkCreateDescriptorSetLayout(device,&descriptor_layout,nullptr,&(sds[i].layout))!=VK_SUCCESS)
            return(false);

        fin_dsl[fin_dsl_count]=sds[i].layout;
        ++fin_dsl_count;
    }

    if(fin_dsl_count<=0)
        return(false);

    //VkPushConstantRange push_constant_range;

    //push_constant_range.stageFlags   = VK_SHADER_STAGE_VERTEX_BIT;
    //push_constant_range.size         = MAX_PUSH_CONSTANT_BYTES;
    //push_constant_range.offset       = 0;
    
    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;

    pPipelineLayoutCreateInfo.setLayoutCount            = fin_dsl_count;
    pPipelineLayoutCreateInfo.pSetLayouts               = fin_dsl;
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 0;//1;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = nullptr;//&push_constant_range;

    if(vkCreatePipelineLayout(device,&pPipelineLayoutCreateInfo,nullptr,&pipeline_layout)!=VK_SUCCESS)
        return(false);

    return(true);
}

DescriptorSets *DescriptorSetLayoutCreater::Create(const DescriptorSetsType &type)const
{
    if(!pipeline_layout)
        return(nullptr);

    ENUM_CLASS_RANGE_ERROR_RETURN_NULLPTR(DescriptorSetsType,type);

    const uint32_t count=sds[(size_t)type].binding_list.GetCount();

    if(!count)
        return(nullptr);

    DescriptorSetAllocateInfo alloc_info;

    alloc_info.descriptorPool       = pool;
    alloc_info.descriptorSetCount   = 1;
    alloc_info.pSetLayouts          = &(sds[(size_t)type].layout);

    VkDescriptorSet desc_set;

    if(vkAllocateDescriptorSets(device,&alloc_info,&desc_set)!=VK_SUCCESS)
        return(nullptr);

    return(new DescriptorSets(device,count,pipeline_layout,desc_set));//,bm));
}
VK_NAMESPACE_END

