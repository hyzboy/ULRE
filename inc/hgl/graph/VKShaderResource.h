﻿#ifndef HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE
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

    const uint32_t *spv_data;
    uint32 spv_size;

    ShaderStageIO stage_io;

public:

    ShaderResource(const VkShaderStageFlagBits &,const uint32_t *,const uint32);
    virtual ~ShaderResource();

    const   VkShaderStageFlagBits   GetStage        ()const {return stage_flag;}
    const   char *                  GetStageName    ()const {return GetShaderStageName(stage_flag);}

    const   uint32_t *              GetSPVData      ()const {return spv_data;}
    const   uint32_t                GetSPVSize      ()const {return spv_size;}

            ShaderAttributeArray &  GetInputs       ()      {return stage_io.input;}
//          ShaderAttributeArray &  GetOutputs      ()      {return stage_io.output;}

    const   uint                    GetInputCount   ()const {return stage_io.input.count;}
//  const   uint                    GetOutputCount  ()const {return stage_io.output.count;}

    const   ShaderAttribute *       GetInput        (const AnsiString &)const;
    const   int                     GetInputBinding (const AnsiString &)const;
};//class ShaderResource
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE
