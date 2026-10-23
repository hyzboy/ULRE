#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include <hgl/mtl/ShaderProgramKey.h>
#include<hgl/log/Log.h>
#include<unordered_set>

namespace hgl::graph{

class IGPUBuffer;
class GeometryVertexFormat;

namespace mtl {}
namespace mtl { class ShaderBuildContext; }


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

    ShaderModuleMap *shader_maps;

    /// 材质是否需要 per-instance 运行时数据行（见 ShaderBuildContext 同名字段）
    bool requires_recipe_runtime_rows = false;
    mtl::ShaderProgramKey program_key;

    ShaderStageCreateInfoList shader_stage_list;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;   ///<共享全局 pipeline layout(设备级单例,不拥有)

private:

    friend class ShaderProgramManager;

    ShaderProgram(const AnsiString &,const mtl::ShaderBuildContext *);

public:

    virtual ~ShaderProgram();

    const   AnsiString &                        GetName                 ()const{return name;}
    const   bool                                    RequiresRecipeRuntimeRows    ()const{return requires_recipe_runtime_rows;}
    const   mtl::ShaderProgramKey &             GetProgramKey          ()const{return program_key;}

    const   PrimitiveType &                     GetPrimitiveType        ()const{return geometry;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return shader_stage_list;}

    const   VkPipelineLayout                    GetPipelineLayout       ()const;

public:


};//class ShaderProgram

using MaterialSet=std::unordered_set<ShaderProgram *>;
}//namespace hgl::graph
