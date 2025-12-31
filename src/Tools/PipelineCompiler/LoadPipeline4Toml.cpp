#include<hgl/graph/VKPipelineData.h>
#include<hgl/graph/VKString.h>
#include<hgl/io/LoadString.h>
#include<toml.hpp>

VK_NAMESPACE_BEGIN

namespace
{
    template<typename E> const bool String2VkEnum(E &result,toml::value &tv,const char *name)
    {
        if(!tv.contains(name))
            return(false);

        result=VK_NAMESPACE::String2VkEnum<E>(tv[name].as_string().c_str());
        return(true);
    }

    void VkToBool(VkBool32 &result,toml::value &tv,const char *name)
    {
        if(!tv.contains(name))
            return;

        const toml::value &v=tv[name];

        if(v.is_boolean())
            result=v.as_boolean();
        else
        if(v.is_string())
        {
            char ch=*(tv[name].as_string().c_str());

            if(ch=='t'||ch=='T'
             ||ch=='y'||ch=='Y'
             ||ch=='1')
                result=VK_TRUE;
            else
                result=VK_FALSE;
        }
    }

    bool LoadFromToml(VkPipelineTessellationStateCreateInfo *tsci,toml::value &tv)
    {
        tsci->patchControlPoints=tv["PatchControlPoints"].as_integer();

        return(true);
    }

    bool LoadFromToml(VkPipelineRasterizationStateCreateInfo *rsci,toml::value &tv)
    {
        VkToBool(       rsci->depthClampEnable,        tv,"DepthClamp");
        VkToBool(       rsci->rasterizerDiscardEnable, tv,"Discard");
        String2VkEnum(  rsci->polygonMode,             tv,"PolygonMode");
        String2VkEnum(  rsci->frontFace,               tv,"FrontFace");

        rsci->cullMode=String2VkCullMode(tv["Cull"].as_string().c_str());

        if(tv.contains("DepthBias"))
        {
            toml::value &depth_bias=tv["DepthBias"];

            rsci->depthBiasEnable        =depth_bias["Enable"          ].as_boolean();
            rsci->depthBiasConstantFactor=depth_bias["ConstantFactor"  ].as_floating();
            rsci->depthBiasClamp         =depth_bias["Clamp"           ].as_floating();
            rsci->depthBiasSlopeFactor   =depth_bias["SlopeFactor"     ].as_floating();
        }

        rsci->lineWidth           =tv["LineWidth"  ].as_floating();

        return(true);
    }

    bool LoadFromToml(VkPipelineMultisampleStateCreateInfo *msci,toml::value &tv)
    {
        msci->rasterizationSamples  =(VkSampleCountFlagBits)tv["Samples"            ].as_integer();

        msci->sampleShadingEnable   =tv["SampleShading"     ].as_boolean();
        msci->minSampleShading      =tv["MinSampleShading"  ].as_floating();
        //msci->pSampleMask
        msci->alphaToCoverageEnable =tv["AlphaToCoverage"   ].as_boolean();
        msci->alphaToOneEnable      =tv["AlphaToOne"        ].as_boolean();

        return(true);
    }

    bool LoadFromToml(VkStencilOpState *sos,toml::value &tv)
    {
        String2VkEnum(sos->failOp,      tv,"FailOp");
        String2VkEnum(sos->passOp,      tv,"PassOp");
        String2VkEnum(sos->depthFailOp, tv,"DepthFailOp");
        String2VkEnum(sos->compareOp,   tv,"CompareOp");

        sos->compareMask=tv["CompareMask"].as_integer();
        sos->writeMask  =tv["WriteMask"  ].as_integer();
        sos->reference  =tv["Reference"  ].as_integer();

        return(true);
    }

    bool LoadFromToml(VkPipelineDepthStencilStateCreateInfo *dssci,toml::value &tv)
    {
        dssci->depthTestEnable      =tv["DepthTest"].as_boolean();
        dssci->depthWriteEnable     =tv["DepthWrite"].as_boolean();

        String2VkEnum(dssci->depthCompareOp,tv,"DepthCompareOp");

        dssci->depthBoundsTestEnable=tv["DepthBoundsTest"].as_boolean();
        dssci->stencilTestEnable    =tv["StencilTest"].as_boolean();

        if(tv.contains("DepthBounds"))
        {
            toml::value &depth_bounds=tv["DepthBounds"];

            dssci->minDepthBounds=depth_bounds["Min"].as_floating();
            dssci->maxDepthBounds=depth_bounds["Max"].as_floating();
        }

        if(tv.contains("Front"))
            LoadFromToml(&(dssci->front),tv["Front"]);

        if(tv.contains("Back"))
            LoadFromToml(&(dssci->back),tv["Back"]);

        return(true);
    }

    bool LoadFromToml(VkPipelineColorBlendAttachmentState *cbas,toml::value &tv)
    {
        cbas->blendEnable=tv["Enable"].as_boolean();

        if(tv.contains("Color"))
        {
            toml::value &color=tv["Color"];
            String2VkEnum(cbas->srcColorBlendFactor,color,"Src");
            String2VkEnum(cbas->dstColorBlendFactor,color,"Dst");
            String2VkEnum(cbas->colorBlendOp,       color,"Op");
        }

        if(tv.contains("Alpha"))
        {
            toml::value &alpha=tv["Alpha"];
        
            String2VkEnum(cbas->srcAlphaBlendFactor,alpha,"Src");
            String2VkEnum(cbas->dstAlphaBlendFactor,alpha,"Dst");
            String2VkEnum(cbas->alphaBlendOp,       alpha,"Op");
        }

        {
            std::string &color_mask=tv["ColorWriteMask"].as_string();

            cbas->colorWriteMask=0;

            for(char ch:color_mask)
            {
                if(ch=='R'||ch=='r')cbas->colorWriteMask|=VK_COLOR_COMPONENT_R_BIT;else
                if(ch=='G'||ch=='g')cbas->colorWriteMask|=VK_COLOR_COMPONENT_G_BIT;else
                if(ch=='B'||ch=='b')cbas->colorWriteMask|=VK_COLOR_COMPONENT_B_BIT;else
                if(ch=='A'||ch=='a')cbas->colorWriteMask|=VK_COLOR_COMPONENT_A_BIT;
            }
        }

        return(true);
    }

    bool LoadFromToml(PipelineData *data,VkPipelineColorBlendStateCreateInfo *cbsci,toml::value &tv)
    {
        if(tv.contains("LogicOp"))
        {
            toml::value &logic_op=tv["LogicOp"];

            cbsci->logicOpEnable=logic_op["Enable"].as_boolean();

            String2VkEnum(cbsci->logicOp,logic_op,"Op");
        }

        if(tv.contains("Attachments"))
        {
            toml::array &attachments=tv["Attachments"].as_array();

            cbsci->attachmentCount=(uint32_t)attachments.size();

            data->SetColorAttachments(cbsci->attachmentCount);

            for(uint i=0;i<cbsci->attachmentCount;i++)
                LoadFromToml(data->color_blend_attachments+i,attachments[i]);
        }

        if(tv.contains("BlendConstants"))
        {
            toml::array &blend_constants=tv["BlendConstants"].as_array();

            cbsci->blendConstants[0]=blend_constants[0].as_floating();
            cbsci->blendConstants[1]=blend_constants[1].as_floating();
            cbsci->blendConstants[2]=blend_constants[2].as_floating();
            cbsci->blendConstants[3]=blend_constants[3].as_floating();
        }

        return(true);
    }
}//namespace

bool LoadPipelineFromToml(PipelineData *pd,const std::string &toml_string)
{
    toml::value root=toml::parse_str(toml_string);

    if(!root.contains("Tessellation"))
        return(false);
    if(!LoadFromToml(pd->tessellation,root["Tessellation"]))
        return(false);

    if(!root.contains("Rasterization"))
        return(false);
    if(!LoadFromToml(pd->rasterization,root["Rasterization"]))
        return(false);

    if(!root.contains("Multisample"))
        return(false);
    if(!LoadFromToml(pd->multi_sample,root["Multisample"]))
        return(false);

    if(!root.contains("DepthStencil"))
        return(false);
    if(!LoadFromToml(pd->depth_stencil,root["DepthStencil"]))
        return(false);

    if(!root.contains("ColorBlend"))
        return(false);
    if(!LoadFromToml(pd,pd->color_blend,root["ColorBlend"]))
        return(false);

    return(true);
}

bool LoadPipelineFromTomlFile(PipelineData *pd,const OSString &filename)
{
    if(!pd)return(false);

    if(filename.IsEmpty())return(false);

    std::string toml_string;

    if(LoadStringFromTextFile(toml_string,filename)<=0)
        return(false);

    return LoadPipelineFromToml(pd,toml_string);
}

VK_NAMESPACE_END
