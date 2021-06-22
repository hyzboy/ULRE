#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/shader/ShaderResource.h>
#include<hgl/type/Map.h>
#include<hgl/type/Sets.h>
VK_NAMESPACE_BEGIN
class DescriptorSets;

/**
* 描述符合集创造器
*/
class DescriptorSetLayoutCreater
{
    VkDevice device;
    VkDescriptorPool pool;

    Sets<uint32_t> all_set;
    Sets<uint32_t> all_binding;

    struct ShaderDescriptorSet
    {
        List<VkDescriptorSetLayoutBinding>  binding_list;
        VkDescriptorSetLayout               layout;
    };

    ShaderDescriptorSet sds[size_t(DescriptorSetType::RANGE_SIZE)];

    VkDescriptorSetLayout fin_dsl[size_t(DescriptorSetType::RANGE_SIZE)];
    uint32_t fin_dsl_count;

    VkPipelineLayout pipeline_layout=VK_NULL_HANDLE;

public:

    DescriptorSetLayoutCreater(VkDevice dev,VkDescriptorPool dp)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,i)
            sds[i].layout=nullptr;

        hgl_zero(fin_dsl);
        fin_dsl_count=0;
        device=dev;pool=dp;
    }
    ~DescriptorSetLayoutCreater();

    void Bind(const ShaderDescriptorList *sd_list,VkDescriptorType type,VkShaderStageFlagBits stage);

    void Bind(const ShaderDescriptorList *sdl,VkShaderStageFlagBits stage)
    {
        for(uint32_t i=VK_DESCRIPTOR_TYPE_BEGIN_RANGE;i<=VK_DESCRIPTOR_TYPE_END_RANGE;i++)
        {
            if(sdl->GetCount()>0)
                Bind(sdl,(VkDescriptorType)i,stage);

            ++sdl;
        }
    }

//以下代码不再需要，使用一个void Bind(const ShaderResource &sr,VkShaderStageFlagBits stage)即可全部替代，而且更方便，但以此为提示
//
//#define DESC_SET_BIND_FUNC(name,vkname) void Bind##name(const uint32_t binding,VkShaderStageFlagBits stage_flag){Bind(binding,VK_DESCRIPTOR_TYPE_##vkname,stage_flag);} \
//                                        void Bind##name(const uint32_t *binding,const uint32_t count,VkShaderStageFlagBits stage_flag){Bind(binding,count,VK_DESCRIPTOR_TYPE_##vkname,stage_flag);}
//
//    DESC_SET_BIND_FUNC(Sampler,                 SAMPLER);
//
//    DESC_SET_BIND_FUNC(CombinedImageSampler,    COMBINED_IMAGE_SAMPLER);
//    DESC_SET_BIND_FUNC(SampledImage,            SAMPLED_IMAGE);
//    DESC_SET_BIND_FUNC(StorageImage,            STORAGE_IMAGE);
//
//    DESC_SET_BIND_FUNC(UTBO,                    UNIFORM_TEXEL_BUFFER);
//    DESC_SET_BIND_FUNC(SSTBO,                   STORAGE_TEXEL_BUFFER);
//    DESC_SET_BIND_FUNC(UBO,                     UNIFORM_BUFFER);
//    DESC_SET_BIND_FUNC(SSBO,                    STORAGE_BUFFER);
//    DESC_SET_BIND_FUNC(UBODynamic,              UNIFORM_BUFFER_DYNAMIC);
//    DESC_SET_BIND_FUNC(SSBODynamic,             STORAGE_BUFFER_DYNAMIC);
//
//    DESC_SET_BIND_FUNC(InputAttachment,         INPUT_ATTACHMENT);
//
//#undef DESC_SET_BIND_FUNC

    bool CreatePipelineLayout();

    const VkPipelineLayout GetPipelineLayout()const{return pipeline_layout;}

    DescriptorSets *Create(const DescriptorSetType &type)const;
};//class DescriptorSetLayoutCreater
VK_NAMESPACE_END