#include<hgl/vk/pipeline/VKGraphicsPipelineData.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{

// ─── GraphicsPipelineData 在 Graphics Pipeline Library (GPL) 路径中的作用 ───────
//
// 虽然 GPL 将 pipeline 分解为四个独立的库 stage（Vertex Input、Pre-Rasterization、
// Fragment Shader、Fragment Output），但 GraphicsPipelineData 预设仍然被使用。
//
// 各 stage 从 GraphicsPipelineData 中读取的内容：
//
//   1. CreatePRLibrary (Pre-Rasterization Stage)
//      读取: pd->rasterization
//      内容: cullMode（正反面剔除）、polygonMode（填充模式）、rasterizerDiscardEnable、深度偏移等
//      注意: rasterizerDiscardEnable=VK_TRUE 会完全禁用光栅化，导致零 fragments 生成。
//            Alpha 测试（discard）必须在 fragment shader 中完成，不应设置此标志。
//
//   2. CreateFSLibrary (Fragment Shader Stage)
//      读取: pd->depth_stencil、pd->multi_sample
//      内容: depthCompareOp、depthWriteEnable、alphaToCoverageEnable 等
//
//   3. CreateFOLibrary (Fragment Output Stage)
//      读取: pd->color_blend
//      内容: blendOp、srcBlend、dstBlend、separateAlpha 等
//
//   4. CreateVILibrary (Vertex Input Stage)
//      不读取 GraphicsPipelineData
//
// 设计原则：
// - Masked3D、Dither3D 等 alpha-test 模式不应设置 rasterizerDiscardEnable
// - AlphaToCoverage3D 应设置 alphaToCoverageEnable 和 MSAA 采样数
// - Color blend 需要根据模式（Alpha、Opaque 等）配置

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
            // Masked3D: alpha discard is done in the fragment shader via 'discard'.
            // rasterizerDiscardEnable must NOT be set — that kills all rasterization.
            pd = new GraphicsPipelineData(1);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Masked3D, pd);
        }

        {
            // Dither3D: same as Masked3D — discard is shader-only.
            pd = new GraphicsPipelineData(1);
            graphics_pipeline_preset_data.Add(GraphicsPipelinePreset::Dither3D, pd);
        }

        {
            // AlphaToCoverage3D: uses MSAA alphaToCoverageEnable.
            // rasterizerDiscardEnable must NOT be set.
            pd = new GraphicsPipelineData(1);
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
