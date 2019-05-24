#ifndef HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/io/DataOutputStream.h>
VK_NAMESPACE_BEGIN
using PipelineID=uint64;

class Pipeline
{
    VkDevice device;
    VkPipeline pipeline;

    bool alpha_test;
    bool alpha_blend;

public:

    Pipeline(VkDevice dev,VkPipeline p,bool at,bool ab):device(dev),pipeline(p),alpha_test(at),alpha_blend(ab){}
    virtual ~Pipeline();

    operator VkPipeline(){return pipeline;}

    const bool IsAlphaTest()const{return alpha_test;}
    const bool IsAlphaBlend()const{return alpha_blend;}
};//class GraphicsPipeline

constexpr size_t MAX_SAMPLE_MASK_COUNT=(VK_SAMPLE_COUNT_64_BIT+31)/32;

class PipelineCreater
{
    VkDevice device;
    VkExtent2D extent;
    VkPipelineCache cache;
    VkGraphicsPipelineCreateInfo pipelineInfo;

    VkPipelineVertexInputStateCreateInfo vis_create_info;

    void InitVertexInputState(const Material *);

    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineTessellationStateCreateInfo tessellation;

    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewportState;

    void InitViewportState();

    VkPipelineRasterizationStateCreateInfo rasterizer;

    VkPipelineMultisampleStateCreateInfo multisampling;
    VkSampleMask sample_mask[MAX_SAMPLE_MASK_COUNT];

    VkPipelineDepthStencilStateCreateInfo depthStencilState;

    VkPipelineColorBlendStateCreateInfo colorBlending;
    List<VkPipelineColorBlendAttachmentState> colorBlendAttachments;

    VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
    VkPipelineDynamicStateCreateInfo dynamicState;

    void InitDynamicState();

    float alpha_test=0;
    bool alpha_blend=false;

public:

    PipelineCreater(Device *dev,const Material *,RenderPass *rp,const VkExtent2D &);
    PipelineCreater(Device *dev,const Material *,RenderPass *rp,const VkExtent2D &,uchar *,uint);
    ~PipelineCreater()=default;

    bool Set(const VkPrimitiveTopology,bool=false);

    void SetViewport(   float x,float y,float w,float h){viewport.x=x;viewport.y=y;viewport.width=w;viewport.height=h;}
    void SetDepthRange( float min_depth,float max_depth){viewport.minDepth=min_depth;viewport.maxDepth=max_depth;}
    void SetScissor(    float l,float t,float w,float h){scissor.offset.x=l;scissor.offset.y=t;scissor.extent.width=w;scissor.extent.height=h;}

    void SetAlphaTest(  const float at)                 {alpha_test=at;}

    void SetDepthTest(  bool                dt)         {depthStencilState.depthTestEnable=dt;}
    void SetDepthWrite( bool                dw)         {depthStencilState.depthWriteEnable=dw;}

    void SetDepthClamp( bool                dc)         {rasterizer.depthClampEnable=dc;}
    void SetDiscard(    bool                discard)    {rasterizer.rasterizerDiscardEnable=discard;}
    void SetPolygonMode(VkPolygonMode       pm)         {rasterizer.polygonMode =pm;}
    void SetCullMode(   VkCullModeFlagBits  cm)         {rasterizer.cullMode    =cm;}
    void CloseCullFace()                                {rasterizer.cullMode    =VK_CULL_MODE_NONE;}
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

    bool SetColorWriteMask(uint index,bool r,bool g,bool b,bool a)
    {
        VkPipelineColorBlendAttachmentState *cba=colorBlendAttachments.GetPointer(index);
        
        if(!cba)return(false);

        cba->colorWriteMask=0;

        if(r)cba->colorWriteMask|=VK_COLOR_COMPONENT_R_BIT;
        if(r)cba->colorWriteMask|=VK_COLOR_COMPONENT_G_BIT;
        if(g)cba->colorWriteMask|=VK_COLOR_COMPONENT_B_BIT;
        if(a)cba->colorWriteMask|=VK_COLOR_COMPONENT_A_BIT;

        return(true);
    }

    void AddColorBlendAttachment(const VkPipelineColorBlendAttachmentState *cba)
    {
        if(!cba)return;

        colorBlendAttachments.Add(*cba);
        colorBlending.attachmentCount=colorBlendAttachments.GetCount();

        if(cba->blendEnable)
            alpha_blend=true;
    }

    bool SetBlend(uint index,bool blend)
    {
        VkPipelineColorBlendAttachmentState *cba=colorBlendAttachments.GetPointer(index);

        if(!cba)return(false);

        cba->blendEnable=blend;

        if(blend)
            alpha_blend=true;
        else
        {
            cba=colorBlendAttachments.GetData();

            for(int i=0;i<colorBlendAttachments.GetCount();i++)
                if(cba->blendEnable)
                {
                    alpha_blend=true;
                    return(true);
                }

            alpha_blend=false;
        }

        return(true);
    }

    void SetLogicOp(VkLogicOp logic_op) {colorBlending.logicOpEnable=VK_TRUE;colorBlending.logicOp=logic_op;}
    void DisableLogicOp()               {colorBlending.logicOpEnable=VK_FALSE;}

    void SetBlendConstans(float r,float g,float b,float a)
    {
        colorBlending.blendConstants[0] = r;
        colorBlending.blendConstants[1] = g;
        colorBlending.blendConstants[2] = b;
        colorBlending.blendConstants[3] = a;
    }

    void SetBlendConstans(float *blend_constans)        {hgl_typecpy(colorBlending.blendConstants,blend_constans,4);}

    bool SaveToStream(io::DataOutputStream *dos);
    bool LoadFromMemory(uchar *,uint);
    
    Pipeline *Create();
};//class PipelineCreater
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
