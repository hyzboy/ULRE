#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKDescriptorSetType.h>
VK_NAMESPACE_BEGIN
using ShaderStageCreateInfoList=List<VkPipelineShaderStageCreateInfo>;

struct MaterialData
{
    UTF8String name;

    VertexInput *vertex_input;

    ShaderModuleMap *shader_maps;

    MaterialDescriptorManager *desc_manager;

    ShaderStageCreateInfoList shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    MaterialParameters *mp_array[DESCRIPTOR_SET_TYPE_COUNT];

    uint32_t mi_data_bytes;

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

public:

    Material(MaterialData *md):data(md){}
    virtual ~Material();

    const   UTF8String &                        GetName                 ()const{return data->name;}

    const   VertexInput *                       GetVertexInput          ()const{return data->vertex_input;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return data->shader_stage_list;}

    const   MaterialDescriptorManager *         GetDescriptorSets       ()const{return data->desc_manager;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return data->pipeline_layout_data;}

public:

            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                RANGE_CHECK_RETURN_NULLPTR(type)

                return data->mp_array[size_t(type)];
            }

    const   bool                                hasSet                  (const DescriptorSetType &type)const;

            VIL *                               CreateVIL(const VILConfig *format_map=nullptr);
            bool                                Release(VIL *);
    const   uint                                GetVILCount();

public:

    MaterialInstance *CreateMI(const VILConfig *vil_cfg=nullptr);
};//class Material

using MaterialSets=SortedSets<Material *>;
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
