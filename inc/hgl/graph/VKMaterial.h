#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKDescriptorSetType.h>

namespace hgl
{
    class ActiveMemoryBlockManager;
}
VK_NAMESPACE_BEGIN
using ShaderStageCreateInfoList=List<VkPipelineShaderStageCreateInfo>;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater
 */
class Material
{
    AnsiString name;

    VertexInput *vertex_input;

    ShaderModuleMap *shader_maps;

    MaterialDescriptorManager *desc_manager;

    ShaderStageCreateInfoList shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    MaterialParameters *mp_array[DESCRIPTOR_SET_TYPE_COUNT];

    uint32_t mi_data_bytes;             ///<实例数据大小
    uint32_t mi_max_count;              ///<实例一次渲染最大数量限制
    
    ActiveMemoryBlockManager *mi_data_manager;

private:

    friend class RenderResource;

    Material(const AnsiString &);

public:

    virtual ~Material();

    const   UTF8String &                        GetName                 ()const{return name;}

    const   VertexInput *                       GetVertexInput          ()const{return vertex_input;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return shader_stage_list;}

    const   MaterialDescriptorManager *         GetDescriptorSets       ()const{return desc_manager;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return pipeline_layout_data;}

public:

            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                RANGE_CHECK_RETURN_NULLPTR(type)

                return mp_array[size_t(type)];
            }

    const   bool                                hasSet                  (const DescriptorSetType &type)const;
    const   bool                                hasAssign               ()const;

    const   VIL *                               GetDefaultVIL()const;
            VIL *                               CreateVIL(const VILConfig *format_map=nullptr);
            bool                                Release(VIL *);
    const   uint                                GetVILCount();

public:

    bool BindUBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindImageSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler);

    void Update();

public:

    const bool      HasMI           ()const{return mi_data_bytes>0;}
    const uint32_t  GetMIDataBytes  ()const{return mi_data_bytes;}
    const uint32_t  GetMIMaxCount   ()const{return mi_max_count;}

    void ReleaseMI(int);    ///<释放材质实例
    void *GetMIData(int);   ///<取得指定ID号的材质实例数据访问指针

    MaterialInstance *CreateMI(const VILConfig *vil_cfg=nullptr);
};//class Material

using MaterialSets=SortedSets<Material *>;
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
