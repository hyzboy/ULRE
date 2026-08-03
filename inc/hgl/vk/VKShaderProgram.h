#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/mtl/MaterialResourceLayout.h>
#include<hgl/mtl/ShaderBufferSource.h>
#include<hgl/log/Log.h>
#include<unordered_set>

namespace hgl::graph{

class IGPUBuffer;
class GeometryVertexFormat;

namespace mtl
{
    class ShaderProgramBuildSpec;
}

class MaterialParameters;

using ShaderStageCreateInfoList=ValueArray<VkPipelineShaderStageCreateInfo>;

/**
 * 材质程序<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater.
 * 在材质需要用到UBO.SSBO数据情况下，Material不能被用于渲染，需要一个MaterialInstance来提供数据才能进行渲染。所以一般情况下，不使用Material进行渲染。<br>
 */
class ShaderProgram
{
    OBJECT_LOGGER

    AnsiString name;

    PrimitiveType geometry;                       ///<图元类型

    VertexInput *vertex_input;

    ShaderModuleMap *shader_maps;

    MaterialDescriptorManager *desc_manager;
    mtl::MaterialResourceLayout material_resource_layout;

    ShaderStageCreateInfoList shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    MaterialParameters *mp_array[DESCRIPTOR_SET_TYPE_COUNT];

    bool has_l2w_matrix;                ///<是否有LocalToWorld矩阵

private:

    friend class ShaderProgramManager;

    ShaderProgram(const AnsiString &,const mtl::ShaderProgramBuildSpec *);

public:

    virtual ~ShaderProgram();

    const   AnsiString &                        GetName                 ()const{return name;}
    const   mtl::MaterialResourceLayout &              GetMaterialResourceLayout      ()const{return material_resource_layout;}

    const   PrimitiveType &                     GetPrimitiveType        ()const{return geometry;}

    const   VertexInput *                       GetVertexInput          ()const{return vertex_input;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return shader_stage_list;}

//    const   MaterialDescriptorManager *         GetDescriptorManager    ()const{return desc_manager;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   MaterialDescriptorManager *         GetDescriptorManager    ()const{return desc_manager;}
    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return pipeline_layout_data;}

public:

            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                RANGE_CHECK_RETURN_NULLPTR(type)

                return mp_array[size_t(type)];
            }

    const   bool                                hasSet                  (const DescriptorSetType &type)const;

    const   VIL *                               GetDefaultVIL()const;
            VIL *                               CreateVIL(const VILConfig *format_map=nullptr);
            VIL *                               CreateVIL(const GeometryVertexFormat &geometry_vertex_format);
            bool                                Release(VIL *);
    const   uint                                GetVILCount();

public:

    bool BindTexture(const DescriptorSetType &type,const AnsiString &name,Texture *tex);
    bool BindTextureSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler);

    bool BindUBO(const DescriptorSetType &type,const AnsiString &name,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindSSBO(const DescriptorSetType &type,const AnsiString &name,const IGPUBuffer *gpu,bool dynamic=false);

    bool BindUBO(const ShaderBufferDesc *sbd,const IGPUBuffer *gpu,bool dynamic=false)
    {
        return BindUBO(sbd->set_type,sbd->name,gpu,dynamic);
    }

    bool BindSSBO(const ShaderBufferDesc *sbd,const IGPUBuffer *gpu,bool dynamic=false)
    {
        return BindSSBO(sbd->set_type,sbd->name,gpu,dynamic);
    }

    void Update();

public:

    const bool      hasLocalToWorld ()const{return has_l2w_matrix; }

};//class ShaderProgram

using MaterialSet=std::unordered_set<ShaderProgram *>;
}//namespace hgl::graph
