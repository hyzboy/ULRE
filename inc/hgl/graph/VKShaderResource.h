#ifndef HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE

#include<hgl/type/String.h>
#include<hgl/type/List.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

struct ShaderStage
{
    AnsiString          name;
    uint                location;

    VertexAttribType    type;       ///<成份数量(如vec4中的4)

    uint                binding;

    bool                global;     ///<是否全局数据
    bool                dynamic;    ///<是否动态数量
};//struct ShaderStage

using ShaderStageList       =ObjectList<ShaderStage>;

class ShaderResource
{
    VkShaderStageFlagBits stage_flag;

    const void *spv_data;
    uint32 spv_size;

    ShaderStageList stage_inputs;
    ShaderStageList stage_outputs;

public:

    ShaderResource(const VkShaderStageFlagBits &,const void *,const uint32);
    virtual ~ShaderResource()=default;

    const   VkShaderStageFlagBits   GetStage            ()const {return stage_flag;}
    const   os_char *               GetStageName        ()const;

    const   uint32_t *              GetCode             ()const {return (uint32_t *)spv_data;}
    const   uint32_t                GetCodeSize         ()const {return spv_size;}

            ShaderStageList &       GetStageInputs      ()      {return stage_inputs;}
            ShaderStageList &       GetStageOutputs     ()      {return stage_outputs;}

    const   uint                    GetStageInputCount  ()const {return stage_inputs.GetCount();}
    const   uint                    GetStageOutputCount ()const {return stage_outputs.GetCount();}

    const   ShaderStage *           GetStageInput       (const AnsiString &)const;
    const   int                     GetStageInputBinding(const AnsiString &)const;
};//class ShaderResource

ShaderResource *LoadShaderResource(const uint8 *origin_filedata,const int64 filesize);

struct ShaderModuleCreateInfo:public vkstruct_flag<VkShaderModuleCreateInfo,VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO>
{
public:

    ShaderModuleCreateInfo(ShaderResource *sr)
    {
        codeSize=sr->GetCodeSize();
        pCode   =sr->GetCode();
    }
};//struct ShaderModuleCreateInfo
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE