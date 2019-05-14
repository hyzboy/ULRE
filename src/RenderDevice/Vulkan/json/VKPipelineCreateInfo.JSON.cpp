#include<json/json.h>
#include<vulkan/vulkan.h>
#include<hgl/type/BaseString.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/util/JsonTool.h>

using namespace hgl;

namespace
{
    #define TO_JSON_BEGIN(struct_name) Json::Value ToJSON(const struct_name *state) \
                                    {   \
                                        Json::Value root; 

    #define TO_JSON_END                    return root;    \
                                    }

    #define TO_JSON_STRUCT(name)   root[#name]=ToJSON(&(state->name));
    #define TO_JSON_OBJECT(name)   root[#name]=ToJSON(state->name);
    #define TO_JSON_BOOL(name)     root[#name]=bool(state->name);
    #define TO_JSON_INT32(name)    root[#name]=Json::Int(state->name);
    #define TO_JSON_UINT32(name)   root[#name]=Json::UInt(state->name);
    #define TO_JSON_INT64(name)    root[#name]=Json::Int64(state->name);
    #define TO_JSON_UINT64(name)   root[#name]=Json::UInt64(state->name);
    #define TO_JSON_FLOAT(name)    root[#name]=state->name;
    #define TO_JSON_STRING(name)   root[#name]=state->name;

    #define FROM_JSON_BEGIN(struct_name)    bool FromJSON(const Json::Value &root,struct_name *state) \
                                            {   \

    #define FROM_JSON_END                       return true;    \
                                            }

    #define CHECK_MEMBER(name)      if(!root.isMember(#name))return(false)
    #define FROM_JSON_STRUCT(name)  CHECK_MEMBER(name);if(!FromJSON(root[#name],&(state->name)))return(false);
    #define FROM_JSON_OBJECT(name)  CHECK_MEMBER(name);if(!FromJSON(root[#name],state->name))return(false);
    #define FROM_JSON_BOOL(name)    CHECK_MEMBER(name);state->name=root[#name].asBool();
    #define FROM_JSON_ENUM(type,name)  CHECK_MEMBER(name);state->name=type(root[#name].asUInt());
    #define FROM_JSON_INT32(name)   CHECK_MEMBER(name);state->name=root[#name].asInt();
    #define FROM_JSON_UINT32(name)  CHECK_MEMBER(name);state->name=root[#name].asUInt();
    #define FROM_JSON_INT64(name)   CHECK_MEMBER(name);state->name=root[#name].asInt64();
    #define FROM_JSON_UINT64(name)  CHECK_MEMBER(name);state->name=root[#name].asUInt64();
    #define FROM_JSON_FLOAT(name)   CHECK_MEMBER(name);state->name=root[#name].asFloat();
    #define FROM_JSON_STRING(name)  CHECK_MEMBER(name);state->name=root[#name].asCString();

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

    #define TO_JSON_ARRAY(name,count)    root[#name]=ToJSON_Array(state->name,count);

    template<typename T> void FromJSON_Array(const Json::Value &root,T *object,const uint count);
    template<> void FromJSON_Array(const Json::Value &root,float *object,const uint count)
    {
        for(uint i=0;i<count;i++)
        {
            *object=root[i].asFloat();
            ++object;
        }
    }

    #define FROM_JSON_ARRAY(name,count) FromJSON_Array(root[#name],state->name,count);

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

    #define TO_JSON_STRUCT_ARRAY(name,count)   root[#name]=ToJSON_StructArray(state->name,count)

    template<typename T>
    void FromJSON_StructArray(const Json::Value &root,T *objects,const uint count)
    {
        for(uint i=0;i<count;i++)
        {
            FromJSON(root[i],objects);
            ++objects;
        }
    }

    #define FROM_JSON_STRUCT_ARRAY(name,count)  FromJSON_StructArray(root[#name],state->name,count);

    TO_JSON_BEGIN(VkStencilOpState)
        TO_JSON_UINT32(failOp)
        TO_JSON_UINT32(passOp)
        TO_JSON_UINT32(depthFailOp)
        TO_JSON_UINT32(compareOp)
        TO_JSON_UINT32(compareMask)
        TO_JSON_UINT32(writeMask)
        TO_JSON_UINT32(reference)
    TO_JSON_END

    FROM_JSON_BEGIN(VkStencilOpState)
        FROM_JSON_ENUM(VkStencilOp,failOp)
        FROM_JSON_ENUM(VkStencilOp,passOp)
        FROM_JSON_ENUM(VkStencilOp,depthFailOp)
        FROM_JSON_ENUM(VkCompareOp,compareOp)
        FROM_JSON_UINT32(compareMask)
        FROM_JSON_UINT32(writeMask)
        FROM_JSON_UINT32(reference)
    FROM_JSON_END

    TO_JSON_BEGIN(VkPipelineDepthStencilStateCreateInfo)
        TO_JSON_BOOL   (depthTestEnable)
        TO_JSON_BOOL   (depthWriteEnable)
        TO_JSON_UINT32 (depthCompareOp)
        TO_JSON_BOOL   (depthBoundsTestEnable)
        TO_JSON_BOOL   (stencilTestEnable)
        TO_JSON_STRUCT (front)
        TO_JSON_STRUCT (back)
        TO_JSON_FLOAT  (minDepthBounds)
        TO_JSON_FLOAT  (maxDepthBounds)
    TO_JSON_END

    FROM_JSON_BEGIN(VkPipelineDepthStencilStateCreateInfo)
        FROM_JSON_BOOL  (depthTestEnable)
        FROM_JSON_BOOL  (depthWriteEnable)
        FROM_JSON_ENUM  (VkCompareOp,depthCompareOp)
        FROM_JSON_BOOL  (depthBoundsTestEnable)
        FROM_JSON_BOOL  (stencilTestEnable)
        FROM_JSON_STRUCT(front)
        FROM_JSON_STRUCT(back)
        FROM_JSON_FLOAT (minDepthBounds)
        FROM_JSON_FLOAT (maxDepthBounds)
    FROM_JSON_END

    TO_JSON_BEGIN(VkPipelineColorBlendAttachmentState)
        TO_JSON_BOOL(blendEnable)
        TO_JSON_UINT32(srcColorBlendFactor)
        TO_JSON_UINT32(dstColorBlendFactor)
        TO_JSON_UINT32(colorBlendOp)
        TO_JSON_UINT32(srcAlphaBlendFactor)
        TO_JSON_UINT32(dstAlphaBlendFactor)
        TO_JSON_UINT32(alphaBlendOp)
        TO_JSON_UINT32(colorWriteMask)
    TO_JSON_END

    FROM_JSON_BEGIN(VkPipelineColorBlendAttachmentState)
        FROM_JSON_BOOL(blendEnable)
        FROM_JSON_ENUM(VkBlendFactor,srcColorBlendFactor)
        FROM_JSON_ENUM(VkBlendFactor,dstColorBlendFactor)
        FROM_JSON_ENUM(VkBlendOp,colorBlendOp)
        FROM_JSON_ENUM(VkBlendFactor,srcAlphaBlendFactor)
        FROM_JSON_ENUM(VkBlendFactor,dstAlphaBlendFactor)
        FROM_JSON_ENUM(VkBlendOp,alphaBlendOp)
        FROM_JSON_UINT32(colorWriteMask)
    FROM_JSON_END

    TO_JSON_BEGIN(VkPipelineColorBlendStateCreateInfo)
        TO_JSON_BOOL(logicOpEnable)
        TO_JSON_UINT32(logicOp)
        TO_JSON_ARRAY(blendConstants,4);
        TO_JSON_UINT32(attachmentCount);
        TO_JSON_STRUCT_ARRAY(pAttachments,state->attachmentCount);
    TO_JSON_END

    FROM_JSON_BEGIN(VkPipelineColorBlendStateCreateInfo)
        FROM_JSON_BOOL(logicOpEnable)
        FROM_JSON_ENUM(VkLogicOp,logicOp)
        FROM_JSON_ARRAY(blendConstants,4);
        FROM_JSON_UINT32(attachmentCount);
        FROM_JSON_STRUCT_ARRAY(pAttachments,state->attachmentCount);
    FROM_JSON_END

    TO_JSON_BEGIN(VkPipelineRasterizationStateCreateInfo)
        TO_JSON_BOOL(depthClampEnable)
        TO_JSON_BOOL(rasterizerDiscardEnable)
        TO_JSON_UINT32(polygonMode)
        TO_JSON_UINT32(cullMode)
        TO_JSON_UINT32(frontFace)
        TO_JSON_BOOL(depthBiasEnable)
        TO_JSON_FLOAT(depthBiasConstantFactor)
        TO_JSON_FLOAT(depthBiasClamp)
        TO_JSON_FLOAT(depthBiasSlopeFactor)
        TO_JSON_FLOAT(lineWidth)
    TO_JSON_END

    TO_JSON_BEGIN(VkPipelineMultisampleStateCreateInfo)
        TO_JSON_UINT32(rasterizationSamples)
        TO_JSON_BOOL(sampleShadingEnable)
        TO_JSON_FLOAT(minSampleShading)
        TO_JSON_BOOL(alphaToCoverageEnable)
        TO_JSON_BOOL(alphaToOneEnable)
    TO_JSON_END 

    TO_JSON_BEGIN(VkPipelineInputAssemblyStateCreateInfo)
        TO_JSON_UINT32(topology)
        TO_JSON_BOOL(primitiveRestartEnable)
    TO_JSON_END

    TO_JSON_BEGIN(VkGraphicsPipelineCreateInfo)
        TO_JSON_OBJECT(pDepthStencilState)
        TO_JSON_OBJECT(pColorBlendState)
        TO_JSON_OBJECT(pRasterizationState)
        TO_JSON_OBJECT(pMultisampleState)
        TO_JSON_OBJECT(pInputAssemblyState)
    TO_JSON_END

    const Json::String  STRING_VER="ver",
                        STRING_VULKAN="vulkan",
                        STRING_PIPELINE="pipeline";
}//namespace

void SaveToJSON(const OSString &filename,const VkGraphicsPipelineCreateInfo *state)
{
    Json::Value root;

    root[STRING_VER]=100;
    root[STRING_VULKAN]=100;
    root[STRING_PIPELINE]=ToJSON(state);
    
    SaveJson(root,filename);
}

bool LoadFromJSON(const OSString &filename,const VkGraphicsPipelineCreateInfo *state)
{
    Json::Value root;

    if(!LoadJson(root,filename))
        return(false);

    if(!root.isMember(STRING_VER))       return(false);
    if(!root.isMember(STRING_VULKAN))    return(false);
    if(!root.isMember(STRING_PIPELINE))  return(false);

    const uint  file_ver    =root[STRING_VER].asUInt(),
                vulkan_ver  =root[STRING_VULKAN].asUInt();

    FromJSON(root[STRING_PIPELINE],state);
}
