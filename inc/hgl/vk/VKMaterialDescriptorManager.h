#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKShaderDescriptorSet.h>

namespace hgl::graph{
class MaterialDescriptorManager
{
    std::string mtl_name;

    int ubo_binding_map[DESCRIPTOR_SET_TYPE_COUNT][mtl::UBODescriptorSemanticCount][2];
    int ssbo_binding_map[DESCRIPTOR_SET_TYPE_COUNT][mtl::SSBODescriptorSemanticCount][2];
    int texture_binding_map[DESCRIPTOR_SET_TYPE_COUNT][mtl::SamplerSlotCount];
    int texture_sampler_binding_map[DESCRIPTOR_SET_TYPE_COUNT][mtl::SamplerSlotCount];

private:

    void InitEnumBindingMaps();
    void RegisterEnumBinding(const ShaderDescriptor *sd);

    VkDescriptorSetLayoutBinding *all_dslb;

    DescriptorSetLayoutCreateInfo dsl_ci[DESCRIPTOR_SET_TYPE_COUNT];

public:

    MaterialDescriptorManager(const std::string &,ShaderDescriptor *,const uint);
    MaterialDescriptorManager(const std::string &,const ShaderDescriptorSetArray &);
    ~MaterialDescriptorManager();

    const std::string &GetMaterialName()const{return mtl_name;}

    const uint GetBindCount(const DescriptorSetType &set_type)const
    {
        RANGE_CHECK_RETURN(set_type,0)

        return dsl_ci[size_t(set_type)].bindingCount;
    }

    const int GetUBO(const DescriptorSetType &set_type,const mtl::UBODescriptorSemantic semantic,bool dynamic)const;
    const int GetSSBO(const DescriptorSetType &set_type,const mtl::SSBODescriptorSemantic semantic,bool dynamic)const;
    const int GetTexture(const DescriptorSetType &set_type,const mtl::SamplerSlot slot)const;
    const int GetTextureSampler(const DescriptorSetType &set_type,const mtl::SamplerSlot slot)const;

    const DescriptorSetLayoutCreateInfo *GetDSLCI(const DescriptorSetType &type)const{return dsl_ci+size_t(type);}

    const bool hasSet(const DescriptorSetType &type)const{return dsl_ci[size_t(type)].bindingCount>0;}
};//class MaterialDescriptorManager
}//namespace hgl::graph
