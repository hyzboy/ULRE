#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKVertexAttributeBinding.h>
VK_NAMESPACE_BEGIN
class DescriptorSetLayoutCreater;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater
 */
class Material
{
    UTF8String mtl_name;

    ShaderModuleMap *shader_maps;
    VertexShaderModule *vertex_sm;
    List<VkPipelineShaderStageCreateInfo> *shader_stage_list;

    DescriptorSetLayoutCreater *dsl_creater;

    DescriptorSets *g_desc_sets;
    DescriptorSets *ri_desc_sets;

    VertexAttributeBinding *vab;

public:

    Material(const UTF8String &name,ShaderModuleMap *smm,List<VkPipelineShaderStageCreateInfo> *,DescriptorSetLayoutCreater *dslc);
    ~Material();

    const UTF8String &          GetName()const{return mtl_name;}

    const VertexShaderModule *  GetVertexShaderModule()const{return vertex_sm;}

    const int                   GetBinding(VkDescriptorType,const AnsiString &)const;

#define GET_BO_BINDING(name,vk_name)    const int Get##name(const AnsiString &obj_name)const{return GetBinding(VK_DESCRIPTOR_TYPE_##vk_name,obj_name);}
//        GET_BO_BINDING(Sampler,             SAMPLER)

        GET_BO_BINDING(Sampler,             COMBINED_IMAGE_SAMPLER)
//        GET_BO_BINDING(SampledImage,        SAMPLED_IMAGE)
        GET_BO_BINDING(StorageImage,        STORAGE_IMAGE)

        GET_BO_BINDING(UTBO,                UNIFORM_TEXEL_BUFFER)
        GET_BO_BINDING(SSTBO,               STORAGE_TEXEL_BUFFER)
        GET_BO_BINDING(UBO,                 UNIFORM_BUFFER)
        GET_BO_BINDING(SSBO,                STORAGE_BUFFER)

        //shader中并不区分普通UBO和动态UBO，所以Material/ShaderResource中的数据，只有UBO

//        GET_BO_BINDING(UBODynamic,          UNIFORM_BUFFER_DYNAMIC)
//        GET_BO_BINDING(SSBODynamic,         STORAGE_BUFFER_DYNAMIC)

        GET_BO_BINDING(InputAttachment,     INPUT_ATTACHMENT)
    #undef GET_BO_BINDING

    const   uint32_t                            GetStageCount           ()const{return shader_stage_list->GetCount();}
    const   VkPipelineShaderStageCreateInfo *   GetStages               ()const{return shader_stage_list->GetData();}

    const   VkPipelineLayout                    GetPipelineLayout       ()const;
            DescriptorSets *                    CreateMIDescriptorSets  ()const;

            DescriptorSets *                    GetGlobalDescriptorSets (){return g_desc_sets;}
            DescriptorSets *                    GetRIDescriptorSets     (){return ri_desc_sets;}
    
    const   VertexAttributeBinding *            GetVAB                  ()const{return vab;}
    const   uint32_t                            GetVertexAttrCount      ()const{return vab->GetVertexAttrCount();}    
    const   VkVertexInputBindingDescription *   GetVertexBindingList    ()const{return vab->GetVertexBindingList();}
    const   VkVertexInputAttributeDescription * GetVertexAttributeList  ()const{return vab->GetVertexAttributeList();}

public:

            MaterialInstance *                  CreateInstance          ();
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
