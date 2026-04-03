#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/pipeline/VKPipelinePreset.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{
PipelineData *LoadPipelineFromFile(const OSString &filename);

namespace
{
    UnorderedMap<PipelinePreset,PipelineData*> inline_pipeline_data;

    UnorderedMap<OSString,PipelineData*> pipeline_data_by_filename;

    void InitPipelinePresetData()
    {
        PipelineData *pd;

        {
            pd=new PipelineData(1);
            inline_pipeline_data.Add(PipelinePreset::Solid3D,pd);
        }

        {
            pd = new PipelineData(1);
            pd->AddDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
            inline_pipeline_data.Add(PipelinePreset::DynamicLineWidth3D,pd);
        }

        {
            pd=new PipelineData(1);
            pd->OpenBlend(0);
            pd->SetColorBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            pd->SetAlphaBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO);
            inline_pipeline_data.Add(PipelinePreset::Alpha3D,pd);
        }

        {
            pd = new PipelineData(1);
            pd->SetAlphaTest(0.5f);
            inline_pipeline_data.Add(PipelinePreset::Masked3D, pd);
        }

        {
            pd = new PipelineData(1);
            pd->SetAlphaTest(0.5f);
            inline_pipeline_data.Add(PipelinePreset::Dither3D, pd);
        }

        {
            pd = new PipelineData(1);
            pd->SetAlphaTest(0.5f);
            pd->SetSamleCount(VK_SAMPLE_COUNT_4_BIT);
            pd->multi_sample->alphaToCoverageEnable = VK_TRUE;
            inline_pipeline_data.Add(PipelinePreset::AlphaToCoverage3D, pd);
        }

        {
            pd=new PipelineData(1);
            pd->CloseCullFace();
            pd->SetDepthTest(true);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
            inline_pipeline_data.Add(PipelinePreset::GizmoOverlay3D,pd);
        }

        {
            pd=new PipelineData(1);
            pd->CloseCullFace();
            pd->SetDepthTest(false);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
            inline_pipeline_data.Add(PipelinePreset::Solid2D,pd);
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
            inline_pipeline_data.Add(PipelinePreset::Alpha2D,pd);
        }

        {
            pd=new PipelineData(1);
            pd->SetCullMode(VK_CULL_MODE_FRONT_BIT);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_GREATER_OR_EQUAL);
            inline_pipeline_data.Add(PipelinePreset::Sky,pd);
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

const PipelineData *GetPipelineData(const PipelinePreset &ip)
{
    if(inline_pipeline_data.GetCount()<=0)
        InitPipelinePresetData();

    PipelineData *pd = nullptr;
    inline_pipeline_data.Get(ip, pd);
    return pd;
}
}//namespace hgl::graph
