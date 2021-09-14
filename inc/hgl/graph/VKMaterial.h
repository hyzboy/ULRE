#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKVertexAttributeBinding.h>
VK_NAMESPACE_BEGIN
struct MaterialData
{
    UTF8String name;

    ShaderModuleMap *shader_maps;
    MaterialDescriptorSets *mds;

    VertexShaderModule *vertex_sm;
    VertexAttributeBinding *vab;

    List<VkPipelineShaderStageCreateInfo> shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    struct
    {
        MaterialParameters *m,*g,*r;
    }mp;

private:

    friend class Material;

    ~MaterialData();
};//struct MaterialData

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater
 */
class Material
{
    MaterialData *data;

private:

    friend GPUDevice;

    MaterialData *GetMaterialData(){return data;}

public:

    Material(MaterialData *md):data(md){}
    ~Material();

    const   UTF8String &                        GetName                 ()const{return data->name;}

    const   VertexShaderModule *                GetVertexShaderModule   ()const{return data->vertex_sm;}

    const   uint32_t                            GetStageCount           ()const{return data->shader_stage_list.GetCount();}
    const   VkPipelineShaderStageCreateInfo *   GetStages               ()const{return data->shader_stage_list.GetData();}

    const   MaterialDescriptorSets *            GetDescriptorSets       ()const{return data->mds;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return data->pipeline_layout_data;}
  
    const   VertexAttributeBinding *            GetVAB                  ()const{return data->vab;}
    const   uint32_t                            GetVertexAttrCount      ()const{return data->vab->GetVertexAttrCount();}    
    const   VkVertexInputBindingDescription *   GetVertexBindingList    ()const{return data->vab->GetVertexBindingList();}
    const   VkVertexInputAttributeDescription * GetVertexAttributeList  ()const{return data->vab->GetVertexAttributeList();}

public:

            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                if(type==DescriptorSetType::Material   )return data->mp.m;else
                if(type==DescriptorSetType::Renderable )return data->mp.r;else
                if(type==DescriptorSetType::Global     )return data->mp.g;else
                return(nullptr);
            }
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
