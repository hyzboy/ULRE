#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
VK_NAMESPACE_BEGIN
using ShaderStageCreateInfoList=List<VkPipelineShaderStageCreateInfo>;

using MaterialParameterArray=MaterialParameters *[size_t(DescriptorSetType::RANGE_SIZE)];

struct MaterialData
{
    UTF8String name;

    ShaderModuleMap *shader_maps;
    MaterialDescriptorSets *mds;

    VertexShaderModule *vertex_sm;

    ShaderStageCreateInfoList shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    MaterialParameterArray mp_array;

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

            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                RANGE_CHECK_RETURN_NULLPTR(type)

                return data->mp_array[size_t(type)];
            }

    const   bool                                hasSet                  (const DescriptorSetType &type)const;
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
