#ifndef HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKDescriptorSet.h>
VK_NAMESPACE_BEGIN
class MaterialParameters
{
    const MaterialDescriptorSets *mds;

    DescriptorSetType ds_type;

    DescriptorSet *descriptor_sets;

private:

    friend class GPUDevice;

    MaterialParameters(const MaterialDescriptorSets *,const DescriptorSetType &type,DescriptorSet *);

public:

    const   DescriptorSetType  GetType             (){return ds_type;}
            DescriptorSet *     GetDescriptorSet    (){return descriptor_sets;}
    const   VkDescriptorSet     GetVkDescriptorSet  ()const{return descriptor_sets->GetDescriptorSet();}

    const   uint32_t            GetCount            ()const{return descriptor_sets->GetCount();}
    const   bool                IsReady             ()const{return descriptor_sets->IsReady();}

public:

    #define MP_TYPE_IS(name)    const   bool is##name()const{return ds_type==DescriptorSetType::name;}
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
    bool BindSampler(const AnsiString &name,Texture *tex,Sampler *sampler);
    bool BindInputAttachment(const AnsiString &name,ImageView *);
    
    void Update();
};//class MaterialParameters
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE