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

    MaterialParameters *mp_m,*mp_g,*mp_r;

    VertexAttributeBinding *vab;

public:

    Material(const UTF8String &name,ShaderModuleMap *smm,List<VkPipelineShaderStageCreateInfo> *,DescriptorSetLayoutCreater *dslc);
    ~Material();

    const   UTF8String &                        GetName                 ()const{return mtl_name;}

    const   VertexShaderModule *                GetVertexShaderModule   ()const{return vertex_sm;}

    const   uint32_t                            GetStageCount           ()const{return shader_stage_list->GetCount();}
    const   VkPipelineShaderStageCreateInfo *   GetStages               ()const{return shader_stage_list->GetData();}

    const   VkPipelineLayout                    GetPipelineLayout       ()const;
  
    const   VertexAttributeBinding *            GetVAB                  ()const{return vab;}
    const   uint32_t                            GetVertexAttrCount      ()const{return vab->GetVertexAttrCount();}    
    const   VkVertexInputBindingDescription *   GetVertexBindingList    ()const{return vab->GetVertexBindingList();}
    const   VkVertexInputAttributeDescription * GetVertexAttributeList  ()const{return vab->GetVertexAttributeList();}

public:

            MaterialParameters *                CreateMP                (const DescriptorSetType &type)const;
            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                if(type==DescriptorSetType::Material   )return mp_m;else
                if(type==DescriptorSetType::Renderable )return mp_r;else
                if(type==DescriptorSetType::Global     )return mp_g;else
                return(nullptr);
            }

            MaterialInstance *                  CreateInstance();
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
