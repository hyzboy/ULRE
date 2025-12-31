#include<hgl/graph/pipeline/VKPipelineData.h>
#include<hgl/graph/pipeline/VKInlinePipeline.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN
PipelineData *LoadPipelineFromFile(const OSString &filename);

namespace
{
    ObjectMap<InlinePipeline,PipelineData> inline_pipeline_data;

    ObjectMap<OSString,PipelineData> pipeline_data_by_filename;

    void InitInlinePipelineData()
    {
        PipelineData *pd;
        
        {
            pd=new PipelineData(1);
            inline_pipeline_data.Add(InlinePipeline::Solid3D,pd);
        }

        {
            pd = new PipelineData(1);
            pd->AddDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
            inline_pipeline_data.Add(InlinePipeline::DynamicLineWidth3D,pd);
        }

        {
            pd=new PipelineData(1);
            pd->OpenBlend(0);
            pd->SetColorBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            pd->SetAlphaBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO);
            inline_pipeline_data.Add(InlinePipeline::Alpha3D,pd);
        }

        {
            pd=new PipelineData(1);
            pd->CloseCullFace();
            pd->SetDepthTest(false);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
            inline_pipeline_data.Add(InlinePipeline::Solid2D,pd);
        }

        {
            pd=new PipelineData(1);
            pd->CloseCullFace();
            pd->SetDepthTest(false);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
            pd->OpenBlend(0);
            pd->SetColorBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            pd->SetAlphaBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO);
            inline_pipeline_data.Add(InlinePipeline::Alpha2D,pd);
        }

        {
            pd=new PipelineData(1);
            pd->SetCullMode(VK_CULL_MODE_FRONT_BIT);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_LESS);
            inline_pipeline_data.Add(InlinePipeline::Sky,pd);
        }
    }
}//namespace

const PipelineData *GetPipelineData(const OSString &filename)
{
    const OSString fn=filename+OS_TEXT(".pipeline");
    
    PipelineData *pd;

    if(pipeline_data_by_filename.Get(fn,pd))
        return pd;

    pd=LoadPipelineFromFile(fn);

    //即便加载入失败了，也放入队列中。避免再次申请加载

    pipeline_data_by_filename.Add(fn,pd);

    return pd;
}

const PipelineData *GetPipelineData(const InlinePipeline &ip)
{
    if(inline_pipeline_data.GetCount()<=0)
        InitInlinePipelineData();

    return inline_pipeline_data[ip];
}
VK_NAMESPACE_END
