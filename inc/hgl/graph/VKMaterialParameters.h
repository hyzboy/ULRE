#ifndef HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKDescriptorSetType.h>
VK_NAMESPACE_BEGIN
class MaterialParameters
{
    const MaterialDescriptorManager *desc_manager;

    DescriptorSetType set_type;

    DescriptorSet *descriptor_set;

private:

    friend class GPUDevice;

    MaterialParameters(const MaterialDescriptorManager *,const DescriptorSetType &type,DescriptorSet *);

public:

    const   DescriptorSetType   GetType             (){return set_type;}
            DescriptorSet *     GetDescriptorSet    (){return descriptor_set;}
    const   VkDescriptorSet     GetVkDescriptorSet  ()const{return descriptor_set->GetDescriptorSet();}

    const   uint32_t            GetCount            ()const{return descriptor_set->GetCount();}
    const   bool                IsReady             ()const{return descriptor_set->IsReady();}

public:

    #define MP_TYPE_IS(name)    const   bool is##name()const{return set_type==DescriptorSetType::name;}
        MP_TYPE_IS(Skeleton)
        MP_TYPE_IS(Instance)
        MP_TYPE_IS(PerObject)
        MP_TYPE_IS(PerMaterial)
        MP_TYPE_IS(PerFrame)
        MP_TYPE_IS(Global)
    #undef MP_TYPE_IS

public:

    virtual ~MaterialParameters();

    bool BindUBO(const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindSSBO(const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindImageSampler(const AnsiString &name,Texture *tex,Sampler *sampler);
    bool BindInputAttachment(const AnsiString &name,ImageView *);
    
    void Update();
};//class MaterialParameters
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE