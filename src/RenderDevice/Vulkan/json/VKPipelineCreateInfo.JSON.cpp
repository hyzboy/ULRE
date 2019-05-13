#include<json/json.h>
#include<vulkan/vulkan.h>
#include<hgl/type/BaseString.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/util/JsonTool.h>

using namespace hgl;

namespace
{
    #define JSON_BEGIN(struct_name) Json::Value ToJSON(const struct_name *state) \
                                    {   \
                                        Json::Value root; 

    #define JSON_END                    return root;    \
                                    }

    #define JSON_STRUCT(name)   root[#name]=ToJSON(&(state->name));
    #define JSON_OBJECT(name)   root[#name]=ToJSON(state->name);
    #define JSON_BOOL(name)     root[#name]=bool(state->name);
    #define JSON_INT32(name)    root[#name]=Json::Int(state->name);
    #define JSON_UINT32(name)   root[#name]=Json::UInt(state->name);
    #define JSON_INT64(name)    root[#name]=Json::Int64(state->name);
    #define JSON_UINT64(name)   root[#name]=Json::UInt64(state->name);
    #define JSON_FLOAT(name)    root[#name]=state->name;
    #define JSON_STRING(name)   root[#name]=state->name;

    template<typename T>
    Json::Value ToJSON_Array(const T *values,const uint count)
    {
        Json::Value items;
    
        for(uint i=0;i<count;i++)
        {
            items[i]=*values;
            ++values;
        }

        return items;
    }

    #define JSON_ARRAY(name,count)    root[#name]=ToJSON_Array(state->name,count);

    template<typename T>
    Json::Value ToJSON_StructArray(const T *object,const uint count)
    {
        Json::Value items;

        for(uint i=0;i<count;i++)
        {
            items[i]=ToJSON(object);
            ++object;
        }

        return items;
    }

    #define JSON_STRUCT_ARRAY(name,count)   root[#name]=ToJSON_StructArray(state->name,count)

    JSON_BEGIN(VkStencilOpState)
        JSON_UINT32(failOp)
        JSON_UINT32(passOp)
        JSON_UINT32(depthFailOp)
        JSON_UINT32(compareOp)
        JSON_UINT32(compareMask)
        JSON_UINT32(writeMask)
        JSON_UINT32(reference)
    JSON_END

    JSON_BEGIN(VkPipelineDepthStencilStateCreateInfo)
        JSON_BOOL   (depthTestEnable)
        JSON_BOOL   (depthWriteEnable)
        JSON_UINT32 (depthCompareOp)
        JSON_BOOL   (depthBoundsTestEnable)
        JSON_BOOL   (stencilTestEnable)
        JSON_STRUCT (front)
        JSON_STRUCT (back)
        JSON_FLOAT  (minDepthBounds)
        JSON_FLOAT  (maxDepthBounds)
    JSON_END

    JSON_BEGIN(VkPipelineColorBlendAttachmentState)
        JSON_BOOL(blendEnable)
        JSON_UINT32(srcColorBlendFactor)
        JSON_UINT32(dstColorBlendFactor)
        JSON_UINT32(colorBlendOp)
        JSON_UINT32(srcAlphaBlendFactor)
        JSON_UINT32(dstAlphaBlendFactor)
        JSON_UINT32(alphaBlendOp)
        JSON_UINT32(colorWriteMask)
    JSON_END

    JSON_BEGIN(VkPipelineColorBlendStateCreateInfo)
        JSON_BOOL(logicOpEnable)
        JSON_UINT32(logicOp)
        JSON_ARRAY(blendConstants,4);
        JSON_STRUCT_ARRAY(pAttachments,state->attachmentCount);
    JSON_END

    JSON_BEGIN(VkPipelineRasterizationStateCreateInfo)
        JSON_BOOL(depthClampEnable)
        JSON_BOOL(rasterizerDiscardEnable)
        JSON_UINT32(polygonMode)
        JSON_UINT32(cullMode)
        JSON_UINT32(frontFace)
        JSON_BOOL(depthBiasEnable)
        JSON_FLOAT(depthBiasConstantFactor)
        JSON_FLOAT(depthBiasClamp)
        JSON_FLOAT(depthBiasSlopeFactor)
        JSON_FLOAT(lineWidth)
    JSON_END

    JSON_BEGIN(VkPipelineMultisampleStateCreateInfo)
        JSON_UINT32(rasterizationSamples)
        JSON_BOOL(sampleShadingEnable)
        JSON_FLOAT(minSampleShading)
        JSON_BOOL(alphaToCoverageEnable)
        JSON_BOOL(alphaToOneEnable)
    JSON_END 

    JSON_BEGIN(VkPipelineInputAssemblyStateCreateInfo)
        JSON_UINT32(topology)
        JSON_BOOL(primitiveRestartEnable)
    JSON_END
}//namespace

void SaveToJSON(const OSString &filename,const VkGraphicsPipelineCreateInfo *state)
{
    Json::Value root;

    root["ver"]=100;
    root["vulkan"]=100;

    JSON_OBJECT(pDepthStencilState)
    JSON_OBJECT(pColorBlendState)
    JSON_OBJECT(pRasterizationState)
    JSON_OBJECT(pMultisampleState)
    JSON_OBJECT(pInputAssemblyState)
    
    SaveJson(root,filename);
}
