#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/SortedSet.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

namespace hgl
{
    class ActiveMemoryBlockManager;
}

VK_NAMESPACE_BEGIN

namespace mtl
{
    class MaterialCreateInfo;
}

using ShaderStageCreateInfoList=ArrayList<VkPipelineShaderStageCreateInfo>;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater.
 * 在材质需要用到UBO.SSBO数据情况下，Material不能被用于渲染，需要一个MaterialInstance来提供数据才能进行渲染。所以一般情况下，不使用Material进行渲染。<br>
 */
class Material
{
    AnsiString name;

    PrimitiveType prim;                       ///<图元类型

    VertexInput *vertex_input;

    ShaderModuleMap *shader_maps;

    MaterialDescriptorManager *desc_manager;

    ShaderStageCreateInfoList shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    MaterialParameters *mp_array[DESCRIPTOR_SET_TYPE_COUNT];

    uint32_t mi_data_bytes;             ///<实例数据大小
    uint32_t mi_max_count;              ///<实例一次渲染最大数量限制
    
    ActiveMemoryBlockManager *mi_data_manager;

    bool has_l2w_matrix;                ///<是否有LocalToWorld矩阵

private:

    friend class MaterialManager;

    Material(const AnsiString &,const mtl::MaterialCreateInfo *);

public:

    virtual ~Material();

    const   AnsiString &                        GetName                 ()const{return name;}

    const   PrimitiveType &                     GetPrimitiveType        ()const{return prim;}

    const   VertexInput *                       GetVertexInput          ()const{return vertex_input;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return shader_stage_list;}

//    const   MaterialDescriptorManager *         GetDescriptorManager    ()const{return desc_manager;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
//    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return pipeline_layout_data;}

public:

            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                RANGE_CHECK_RETURN_NULLPTR(type)

                return mp_array[size_t(type)];
            }

    const   bool                                hasSet                  (const DescriptorSetType &type)const;

    const   VIL *                               GetDefaultVIL()const;
            VIL *                               CreateVIL(const VILConfig *format_map=nullptr);
            bool                                Release(VIL *);
    const   uint                                GetVILCount();

public:

    bool BindUBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindTextureSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler);

    bool BindUBO(const ShaderBufferDesc *sbd,DeviceBuffer *ubo,bool dynamic=false)
    {
        return BindUBO(sbd->set_type,sbd->name,ubo,dynamic);
    }

    void Update();

public:

    const bool      hasLocalToWorld ()const{return has_l2w_matrix; }

    const bool      hasMI           ()const{return mi_data_bytes>0;}
    const uint32_t  GetMIDataBytes  ()const{return mi_data_bytes;}
    const uint32_t  GetMIMaxCount   ()const{return mi_max_count;}

    void ReleaseMI(int);    ///<释放材质实例
    void *GetMIData(int);   ///<取得指定ID号的材质实例数据访问指针

    MaterialInstance *CreateMI(const VIL *);
    MaterialInstance *CreateMI(const VILConfig *vil_cfg=nullptr);
};//class Material

using MaterialSet=SortedSet<Material *>;
VK_NAMESPACE_END
