#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
class MaterialParameters
{
    OBJECT_LOGGER

protected:

    const MaterialDescriptorManager *desc_manager;

    DescriptorSetType set_type;

    DescriptorSet *descriptor_set;

protected:

    bool BindTexture(const int &index,Texture *tex);
    bool BindResourceSampler(const int &index,Texture *tex,Sampler *sampler);
    bool BindInputAttachment(const int &index,ImageView *);

    bool BindUBO(const int &index,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindSSBO(const int &index,const IGPUBuffer *gpu,bool dynamic=false);

public:

    const   DescriptorSetType   GetType             (){return set_type;}
            DescriptorSet *     GetDescriptorSet    (){return descriptor_set;}
    const   VkDescriptorSet     GetVkDescriptorSet  ()const{return descriptor_set->GetDescriptorSet();}

    const   uint32_t            GetDescriptorCount  ()const{return desc_manager->GetBindCount(set_type);}   ///<获取总共需要绑定的描述符数量

    const   uint32_t            GetBoundCount       ()const{return descriptor_set->GetCount();}             ///<获取已经绑好的数量
    const   bool                IsReady             ()const{return descriptor_set->IsReady();}              ///<是否全部绑好了

public:

    const bool isStatic() const { return set_type==DescriptorSetType::Static; }
    const bool isPerFrame() const { return set_type==DescriptorSetType::PerFrame; }
    const bool isPerObject() const { return set_type==DescriptorSetType::PerObject; }
    const bool isPerMaterial() const { return set_type==DescriptorSetType::PerMaterial; }

public:

    MaterialParameters(const MaterialDescriptorManager *,const DescriptorSetType &type,DescriptorSet *);
    virtual ~MaterialParameters();

    bool BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindTexture(const mtl::SamplerSlot slot,Texture *tex);
    bool BindResourceSampler(const mtl::SamplerSlot slot,Texture *tex,Sampler *sampler);

    void Update();
};//class MaterialParameters
}//namespace hgl::graph
