#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKDescriptorSetType.h>
VK_NAMESPACE_BEGIN
using ShaderStageCreateInfoList=List<VkPipelineShaderStageCreateInfo>;

using MaterialParameterArray=MaterialParameters *[DESCRIPTOR_SET_TYPE_COUNT];

struct MaterialData
{
    UTF8String name;

    VertexInput *vertex_input;

    ShaderModuleMap *shader_maps;

    MaterialDescriptorManager *desc_manager;

    ShaderStageCreateInfoList shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    MaterialParameterArray mp_array;

    uint8 *mi_data;         ///<材质实例数据区
    uint32_t mi_size;       ///<单个材质实例数据长度

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

    uint32_t mi_count;      ///<材质数量

private:

    friend GPUDevice;

    MaterialData *GetMaterialData(){return data;}

public:

    Material(MaterialData *);
    virtual ~Material();

    const   UTF8String &                        GetName                 ()const{return data->name;}

    const   VertexInput *                       GetVertexInput          ()const{return data->vertex_input;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return data->shader_stage_list;}

    const   MaterialDescriptorManager *         GetDescriptorSets       ()const{return data->desc_manager;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return data->pipeline_layout_data;}

public:

    const   uint32_t                            GetMICount              ()const{return mi_count;}
    const   uint32_t                            GetMISize               ()const{return data->mi_size;}
    const   void *                              GetMIData               ()const{return data->mi_data;}

    template<typename T>
            T *                                 GetMIData               (const uint32_t index)const
    {
        if(!data->mi_data)return(nullptr);
        if(index>=mi_count)return(nullptr);

        return data->mi_data+index*mi_size;
    }

    template<typename T>
            bool                                WriteMIData             (const uint32_t index,const T *write_data)
    {
        if(!data->mi_data)return(false);
        if(index>=mi_count)return(false);

        memcpy(data->mi_data+index*mi_size,write_data,data->mi_size);
        return(true);
    }

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
};//class Material

using MaterialSets=SortedSets<Material *>;
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
