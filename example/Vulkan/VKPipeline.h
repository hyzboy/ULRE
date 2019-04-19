#ifndef HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_INCLUDE

#include"VK.h"

VK_NAMESPACE_BEGIN
class Device;
class RenderPass;

class Pipeline
{
    VkDevice device;
    VkPipeline pipeline;

public:

    Pipeline(VkDevice dev,VkPipeline p){device=dev;pipeline=p;}
    virtual ~Pipeline();

    operator VkPipeline(){return pipeline;}
};//class GraphicsPipeline

class Shader;
class VertexInput;

class PipelineCreater
{
    VkDevice device;
    VkExtent2D extent;
    VkPipelineCache cache;
    VkGraphicsPipelineCreateInfo pipelineInfo;

    VkPipelineVertexInputStateCreateInfo vis_create_info;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;

    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineDepthStencilStateCreateInfo depthStencilState;

    VkPipelineRasterizationStateCreateInfo rasterizer;

    VkPipelineMultisampleStateCreateInfo multisampling;

    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlending;

    VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
    VkPipelineDynamicStateCreateInfo dynamicState;

private:

    const Shader *        shader      =nullptr;
    const VertexInput *   vertex_input=nullptr;

public:

    PipelineCreater(Device *dev);
    ~PipelineCreater()=default;

    bool Set(const Shader *);
    bool Set(const VertexInput *);
    bool Set(const VkPrimitiveTopology,bool=false);
    bool Set(VkPipelineLayout pl);
    bool Set(VkRenderPass,uint32_t subpass=0);

    void SetViewport(   float x,float y,float w,float h){viewport.x=x;viewport.y=y;viewport.width=w;viewport.height=h;}
    void SetDepthRange( float min_depth,float max_depth){viewport.minDepth=min_depth;viewport.maxDepth=max_depth;}
    void SetScissor(    float l,float t,float w,float h){scissor.offset.x=l;scissor.offset.y=t;scissor.extent.width=w;scissor.extent.height=h;}

    void SetDepthClamp( bool                dc)         {rasterizer.depthClampEnable=dc;}
    void SetDiscard(    bool                discard)    {rasterizer.rasterizerDiscardEnable=discard;}
    void SetPolygonMode(VkPolygonMode       pm)         {rasterizer.polygonMode =pm;}
    void SetCullMode(   VkCullModeFlagBits  cm)         {rasterizer.cullMode    =cm;}
    void SetFrontFace(  VkFrontFace         ff)         {rasterizer.frontFace   =ff;}
    void SetDepthBias(  float               ConstantFactor,
                        float               Clamp,
                        float               SlopeFactor)
    {
        rasterizer.depthBiasEnable          =VK_TRUE;
        rasterizer.depthBiasConstantFactor  =ConstantFactor;
        rasterizer.depthBiasClamp           =Clamp;
        rasterizer.depthBiasSlopeFactor     =SlopeFactor;
    }
    void DisableDepthBias()                             {rasterizer.depthBiasEnable=VK_FALSE;}
    void SetLineWidth(  float               line_width) {rasterizer.lineWidth   =line_width;}

    void SetSamleCount( VkSampleCountFlagBits sc)
    {
        multisampling.sampleShadingEnable=(sc==VK_SAMPLE_COUNT_1_BIT?VK_FALSE:VK_TRUE);
        multisampling.rasterizationSamples=sc;
    }

    void SetColorWriteMask(bool r,bool g,bool b,bool a)
    {
        colorBlendAttachment.colorWriteMask=0;

        if(r)colorBlendAttachment.colorWriteMask|=VK_COLOR_COMPONENT_R_BIT;
        if(r)colorBlendAttachment.colorWriteMask|=VK_COLOR_COMPONENT_G_BIT;
        if(g)colorBlendAttachment.colorWriteMask|=VK_COLOR_COMPONENT_B_BIT;
        if(a)colorBlendAttachment.colorWriteMask|=VK_COLOR_COMPONENT_A_BIT;
    }

    void SetBlend(      bool                blend)      {colorBlendAttachment.blendEnable=blend;}
    void SetLogicOp(    VkLogicOp           logic_op)   {colorBlending.logicOpEnable=VK_TRUE;colorBlending.logicOp=logic_op;}
    void DisableLogicOp()                               {colorBlending.logicOpEnable=VK_FALSE;}

    void SetBlendConstans(float r,float g,float b,float a)
    {
        colorBlending.blendConstants[0] = r;
        colorBlending.blendConstants[1] = g;
        colorBlending.blendConstants[2] = b;
        colorBlending.blendConstants[3] = a;
    }

    void SetBlendConstans(float *blend_constans)        {hgl_typecpy(colorBlending.blendConstants,blend_constans,4);}

    Pipeline *Create();
};//class PipelineCreater
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
