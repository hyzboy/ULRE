#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
VK_NAMESPACE_BEGIN
using ShaderStageCreateInfoList=List<VkPipelineShaderStageCreateInfo>;

struct MaterialData
{
    UTF8String name;

    ShaderModuleMap *shader_maps;
    MaterialDescriptorSets *mds;

    VertexShaderModule *vertex_sm;

    ShaderStageCreateInfoList shader_stage_list;

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

            VertexShaderModule *                GetVertexShaderModule   ()     {return data->vertex_sm;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return data->shader_stage_list;}

    const   MaterialDescriptorSets *            GetDescriptorSets       ()const{return data->mds;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return data->pipeline_layout_data;}

public:

            MaterialParameters *                GetMP                   (const DescriptorSetsType &type)
            {
                if(type==DescriptorSetsType::Material   )return data->mp.m;else
                if(type==DescriptorSetsType::Primitive )return data->mp.r;else
                if(type==DescriptorSetsType::Global     )return data->mp.g;else
                return(nullptr);
            }
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
