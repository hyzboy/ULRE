#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKShaderDescriptorSet.h>

namespace hgl::graph{
class MaterialDescriptorManager
{
    AnsiString mtl_name;

    BindingMapArray binding_map[DESCRIPTOR_SET_TYPE_COUNT];

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

    MaterialDescriptorManager(const AnsiString &,ShaderDescriptor *,const uint);
    MaterialDescriptorManager(const AnsiString &,const ShaderDescriptorSetArray &);
    ~MaterialDescriptorManager();

    const AnsiString &GetMaterialName()const{return mtl_name;}

    const uint GetBindCount(const DescriptorSetType &set_type)const
    {
        RANGE_CHECK_RETURN(set_type,0)

        return dsl_ci[size_t(set_type)].bindingCount;
    }

    const BindingMapArray &GetBindingMap(const DescriptorSetType &set_type)const
    {
        return binding_map[size_t(set_type)];
    }

    const int GetUBO(const DescriptorSetType &set_type,const mtl::UBODescriptorSemantic semantic,bool dynamic)const;
    const int GetSSBO(const DescriptorSetType &set_type,const mtl::SSBODescriptorSemantic semantic,bool dynamic)const;
    const int GetTexture(const DescriptorSetType &set_type,const mtl::SamplerSlot slot)const;
    const int GetTextureSampler(const DescriptorSetType &set_type,const mtl::SamplerSlot slot)const;

    [[deprecated("Use semantic or slot-based GetUBO/GetSSBO/GetTexture instead")]]
    const int GetBinding(const DescriptorSetType &set_type,const VkDescriptorType &desc_type,const AnsiString &name)const;

    [[deprecated("Use GetUBO(set_type, UBODescriptorSemantic, dynamic) instead")]]
    const int GetUBO            (const DescriptorSetType &set_type,const AnsiString &name,bool dynamic)const{return GetBinding(set_type,dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,name);}
    [[deprecated("Use GetSSBO(set_type, SSBODescriptorSemantic, dynamic) instead")]]
    const int GetSSBO           (const DescriptorSetType &set_type,const AnsiString &name,bool dynamic)const{return GetBinding(set_type,dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,name);}
    [[deprecated("Use GetTexture(set_type, SamplerSlot) instead")]]
    const int GetTexture        (const DescriptorSetType &set_type,const AnsiString &name             )const{return GetBinding(set_type,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,name);}
    [[deprecated("Use GetTextureSampler(set_type, SamplerSlot) instead")]]
    const int GetTextureSampler (const DescriptorSetType &set_type,const AnsiString &name             )const{return GetBinding(set_type,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,name);}
    [[deprecated("String-based GetInputAttachment not recommended")]]
    const int GetInputAttachment(const DescriptorSetType &set_type,const AnsiString &name             )const{return GetBinding(set_type,VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,name);}

    const DescriptorSetLayoutCreateInfo *GetDSLCI(const DescriptorSetType &type)const{return dsl_ci+size_t(type);}

    const bool hasSet(const DescriptorSetType &type)const{return dsl_ci[size_t(type)].bindingCount>0;}
};//class MaterialDescriptorManager
}//namespace hgl::graph
