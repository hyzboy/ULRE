#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/vulkan/ShaderModuleMap.h>
#include<hgl/graph/vulkan/VKVertexAttributeBinding.h>
VK_NAMESPACE_BEGIN
class DescriptorSetLayoutCreater;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater
 */
class Material
{
    ShaderModuleMap *shader_maps;
    VertexShaderModule *vertex_sm;
    List<VkPipelineShaderStageCreateInfo> *shader_stage_list;

    DescriptorSetLayoutCreater *dsl_creater;

    VertexAttributeBinding *vab;

public:

    Material(ShaderModuleMap *smm,List<VkPipelineShaderStageCreateInfo> *,DescriptorSetLayoutCreater *dslc);
    ~Material();

    const VertexShaderModule *GetVertexShaderModule()const{return vertex_sm;}

    const int GetBinding(VkDescriptorType,const AnsiString &)const;

#define GET_BO_BINDING(name,vk_name)    const int Get##name(const AnsiString &obj_name)const{return GetBinding(VK_DESCRIPTOR_TYPE_##vk_name,obj_name);}
//        GET_BO_BINDING(Sampler,             SAMPLER)

        GET_BO_BINDING(Sampler,             COMBINED_IMAGE_SAMPLER)
//        GET_BO_BINDING(SampledImage,        SAMPLED_IMAGE)
        GET_BO_BINDING(StorageImage,        STORAGE_IMAGE)

        GET_BO_BINDING(UTBO,                UNIFORM_TEXEL_BUFFER)
        GET_BO_BINDING(SSTBO,               STORAGE_TEXEL_BUFFER)
        GET_BO_BINDING(UBO,                 UNIFORM_BUFFER)
        GET_BO_BINDING(SSBO,                STORAGE_BUFFER)
        GET_BO_BINDING(UBODynamic,          UNIFORM_BUFFER_DYNAMIC)
        GET_BO_BINDING(SSBODynamic,         STORAGE_BUFFER_DYNAMIC)

        GET_BO_BINDING(InputAttachment,     INPUT_ATTACHMENT)
    #undef GET_BO_BINDING

    const   uint32_t                            GetStageCount           ()const{return shader_stage_list->GetCount();}
    const   VkPipelineShaderStageCreateInfo *   GetStages               ()const{return shader_stage_list->GetData();}

    const   VkPipelineLayout                    GetPipelineLayout       ()const;
            DescriptorSets *                    CreateDescriptorSets    ()const;
    
    const   VertexAttributeBinding *            GetVAB                  ()const{return vab;}
    const   uint32_t                            GetVertexAttrCount      ()const{return vab->GetVertexAttrCount();}    
    const   VkVertexInputBindingDescription *   GetVertexBindingList    ()const{return vab->GetVertexBindingList();}
    const   VkVertexInputAttributeDescription * GetVertexAttributeList  ()const{return vab->GetVertexAttributeList();}

public:

            Renderable *                        CreateRenderable        (const uint32_t draw_count=0);
            MaterialInstance *                  CreateInstance          ();
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
