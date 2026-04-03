#include<hgl/vk/pipeline/VKGraphicsPipelineData.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{
GraphicsPipelineData *LoadPipelineFromFile(const OSString &filename);

namespace
{
    UnorderedMap<GraphicsPipelinePreset,GraphicsPipelineData*> graphics_pipeline_preset_data;

    UnorderedMap<OSString,GraphicsPipelineData*> graphics_pipeline_data_by_filename;

    void InitGraphicsPipelinePresetData()
    {
        GraphicsPipelineData *pd;

        {
            pd=new GraphicsPipelineData(1);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Solid3D,pd);
        }

        {
            pd = new GraphicsPipelineData(1);
            pd->AddDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::DynamicLineWidth3D,pd);
        }

        {
            pd=new GraphicsPipelineData(1);
            pd->OpenBlend(0);
            pd->SetColorBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            pd->SetAlphaBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Alpha3D,pd);
        }

        {
            pd = new GraphicsPipelineData(1);
            pd->SetAlphaTest(0.5f);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Masked3D, pd);
        }

        {
            pd = new GraphicsPipelineData(1);
            pd->SetAlphaTest(0.5f);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Dither3D, pd);
        }

        {
            pd = new GraphicsPipelineData(1);
            pd->SetAlphaTest(0.5f);
            pd->SetSamleCount(VK_SAMPLE_COUNT_4_BIT);
            pd->multi_sample->alphaToCoverageEnable = VK_TRUE;
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::AlphaToCoverage3D, pd);
        }

        {
            pd=new GraphicsPipelineData(1);
            pd->CloseCullFace();
            pd->SetDepthTest(true);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::GizmoOverlay3D,pd);
        }

        {
            pd=new GraphicsPipelineData(1);
            pd->CloseCullFace();
            pd->SetDepthTest(false);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Solid2D,pd);
        }

        {
            pd=new GraphicsPipelineData(1);
            pd->CloseCullFace();
            pd->SetDepthTest(false);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
            pd->OpenBlend(0);
            pd->SetColorBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            pd->SetAlphaBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Alpha2D,pd);
        }

        {
            pd=new GraphicsPipelineData(1);
            pd->SetCullMode(VK_CULL_MODE_FRONT_BIT);
            pd->SetDepthWrite(false);
            pd->SetDepthCompareOp(VK_COMPARE_OP_GREATER_OR_EQUAL);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Sky,pd);
        }
    }
}//namespace

const GraphicsPipelineData *GetGraphicsPipelineData(const OSString &filename)
{
    const OSString fn=filename+OS_TEXT(".pipeline");

    GraphicsPipelineData *pd;

    if(graphics_pipeline_data_by_filename.Get(fn,pd))
        return pd;

    pd=LoadPipelineFromFile(fn);

    //即便加载入失败了，也放入队列中。避免再次申请加载

    graphics_pipeline_data_by_filename.Add(fn,pd);

    return pd;
}

const GraphicsPipelineData *GetGraphicsPipelineData(const GraphicsPipelinePreset &ip)
{
    if(graphics_pipeline_preset_data.GetCount()<=0)
        InitGraphicsPipelinePresetData();

    GraphicsPipelineData *pd = nullptr;
    graphics_pipeline_preset_data.Get(ip, pd);
    return pd;
}
}//namespace hgl::graph
