#include<json/json.h>
#include<vulkan/vulkan.h>
#include<hgl/type/BaseString.h>
#include<sstream>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;

#define TOML_BEGIN(struct_name) toml::table ToTOML(const struct_name *state) \
                                {   \
                                    toml::table root=toml::table 

#define TOML_END                    ;return root;    \
                                }

#define TOML_BOOL(name)     {#name,bool(state->##name)}
#define TOML_INTEGER(name)  {#name,std::int64_t(state->##name)}
#define TOML_FLOAT(name)    {#name,double(state->##name)}
#define TOML_STRING(name)   {#name,toml::string(state->##name)}

TOML_BEGIN(VkStencilOpState)
{
    TOML_INTEGER(failOp),
    TOML_INTEGER(passOp),
    TOML_INTEGER(depthFailOp),
    TOML_INTEGER(compareOp),
    TOML_INTEGER(compareMask),
    TOML_INTEGER(writeMask),
    TOML_INTEGER(reference)
}
TOML_END

TOML_BEGIN(VkPipelineDepthStencilStateCreateInfo)
{
    TOML_BOOL   (depthTestEnable),
    TOML_BOOL   (depthWriteEnable),
    TOML_INTEGER(depthCompareOp),
    TOML_BOOL   (depthBoundsTestEnable),
    TOML_BOOL   (stencilTestEnable),
    {"front",   ToTOML(&state->front)},
    {"back",    ToTOML(&state->back)}
}
TOML_END

TOML_BEGIN(VkPipelineColorBlendAttachmentState)
{
    TOML_BOOL(blendEnable),
    TOML_INTEGER(srcColorBlendFactor),
    TOML_INTEGER(dstColorBlendFactor),
    TOML_INTEGER(colorBlendOp),
    TOML_INTEGER(srcAlphaBlendFactor),
    TOML_INTEGER(dstAlphaBlendFactor),
    TOML_INTEGER(alphaBlendOp),
    TOML_INTEGER(colorWriteMask)
}
TOML_END

TOML_BEGIN(VkPipelineColorBlendStateCreateInfo)
{
    TOML_BOOL(logicOpEnable),
    TOML_INTEGER(logicOp),
    {"blendConstants",  toml::array
                        {
                            state->blendConstants[0],
                            state->blendConstants[1],
                            state->blendConstants[2],
                            state->blendConstants[3]
                        }
    }};

    toml::array pAttachments;

    for(uint i=0;i<state->attachmentCount;i++)
        pAttachments.push_back(ToTOML(&(state->pAttachments[i])));

    root.insert({"pAttachments",pAttachments});

TOML_END

TOML_BEGIN(VkPipelineRasterizationStateCreateInfo)
{
    TOML_BOOL(depthClampEnable),
    TOML_BOOL(rasterizerDiscardEnable),
    TOML_INTEGER(polygonMode),
    TOML_INTEGER(cullMode),
    TOML_INTEGER(frontFace),
    TOML_BOOL(depthBiasEnable),
    TOML_FLOAT(depthBiasConstantFactor),
    TOML_FLOAT(depthBiasClamp),
    TOML_FLOAT(depthBiasSlopeFactor),
    TOML_FLOAT(lineWidth)
}
TOML_END

TOML_BEGIN(VkPipelineMultisampleStateCreateInfo)
{
    TOML_INTEGER(rasterizationSamples),
    TOML_BOOL(sampleShadingEnable),
    TOML_FLOAT(minSampleShading),
    TOML_BOOL(alphaToCoverageEnable),
    TOML_BOOL(alphaToOneEnable)
}
TOML_END

TOML_BEGIN(VkPipelineInputAssemblyStateCreateInfo)
{
    Json::Value root;

    const auto root=toml::table
    {
        {"ver",toml::table{{"file",100},{"vulkan",100}}},
        {"pDepthStencilState",  ToTOML(info->pDepthStencilState)},
        {"pColorBlendState",    ToTOML(info->pColorBlendState)},
        {"pRasterizationState", ToTOML(info->pRasterizationState)},
        {"pMultisampleState",   ToTOML(info->pMultisampleState)},
        {"pInputAssemblyState", ToTOML(info->pInputAssemblyState)}
    };

    std::stringstream ss;

    ss<<root;

    const std::string str=ss.str();

    filesystem::SaveMemoryToFile(filename,str.c_str(),str.length());
}
