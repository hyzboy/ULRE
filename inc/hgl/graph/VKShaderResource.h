#ifndef HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE

#include<hgl/type/String.h>
#include<hgl/type/List.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKStruct.h>

VK_NAMESPACE_BEGIN

class ShaderResource
{
    VkShaderStageFlagBits stage_flag;

    const void *spv_data;
    uint32 spv_size;

    ShaderStageIO stage_io;

public:

    ShaderResource(const VkShaderStageFlagBits &,const void *,const uint32);
    virtual ~ShaderResource()=default;

    const   VkShaderStageFlagBits   GetStageFlagBit     ()const {return stage_flag;}
    const   char *                  GetStageFlagName    ()const {return GetShaderStageName(stage_flag);}

    const   uint32_t *              GetCode             ()const {return (uint32_t *)spv_data;}
    const   uint32_t                GetCodeSize         ()const {return spv_size;}

            ShaderAttributeList &   GetStageInputs      ()      {return stage_io.input;}
//          ShaderAttributeList &   GetStageOutputs     ()      {return stage_io.output;}

    const   uint                    GetStageInputCount  ()const {return stage_io.input.GetCount();}
//  const   uint                    GetStageOutputCount ()const {return stage_io.output.GetCount();}

    const   ShaderAttribute *       GetStageInput       (const AnsiString &)const;
    const   int                     GetStageInputBinding(const AnsiString &)const;
};//class ShaderResource

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