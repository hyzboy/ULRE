#ifndef HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE

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

public:

    const   DescriptorSetType   GetType             (){return set_type;}
            DescriptorSet *     GetDescriptorSet    (){return descriptor_set;}
    const   VkDescriptorSet     GetVkDescriptorSet  ()const{return descriptor_set->GetDescriptorSet();}

    const   uint32_t            GetDescriptorCount  ()const{return desc_manager->GetBindCount(set_type);}   ///<获取总共需要绑定的描述符数量
    const   BindingMapArray &   GetBindingMap       ()const{return desc_manager->GetBindingMap(set_type);}

    const   uint32_t            GetBoundCount       ()const{return descriptor_set->GetCount();}             ///<获取已经绑好的数量
    const   bool                IsReady             ()const{return descriptor_set->IsReady();}              ///<是否全部绑好了

public:

    #define MP_TYPE_IS(name)    const   bool is##name()const{return set_type==DescriptorSetType::name;}
        MP_TYPE_IS(Static)
        MP_TYPE_IS(PerFrame)
        MP_TYPE_IS(PerObject)
        MP_TYPE_IS(PerMaterial)
    #undef MP_TYPE_IS

public:

    MaterialParameters(const MaterialDescriptorManager *,const DescriptorSetType &type,DescriptorSet *);
    virtual ~MaterialParameters();

    bool BindTexture(const int &index,Texture *tex);
    bool BindTextureSampler(const int &index,Texture *tex,Sampler *sampler);
    bool BindInputAttachment(const int &index,ImageView *);

    bool BindUBO(const int &index,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindSSBO(const int &index,const IGPUBuffer *gpu,bool dynamic=false);

    bool BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindTexture(const mtl::SamplerSlot slot,Texture *tex);
    bool BindTextureSampler(const mtl::SamplerSlot slot,Texture *tex,Sampler *sampler);

    [[deprecated("Use slot-based overload instead")]]
    bool BindTexture(const AnsiString &name,Texture *tex);
    [[deprecated("Use slot-based overload instead")]]
    bool BindTextureSampler(const AnsiString &name,Texture *tex,Sampler *sampler);
    [[deprecated("Use name lookup overload not recommended")]]
    bool BindInputAttachment(const AnsiString &name,ImageView *);

    [[deprecated("Use semantic-based overload instead")]]
    bool BindUBO(const AnsiString &name,const IGPUBuffer *gpu,bool dynamic=false);
    [[deprecated("Use semantic-based overload instead")]]
    bool BindSSBO(const AnsiString &name,const IGPUBuffer *gpu,bool dynamic=false);

    void Update();
};//class MaterialParameters
}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE
