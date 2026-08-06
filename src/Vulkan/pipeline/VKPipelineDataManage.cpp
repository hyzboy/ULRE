#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/pipeline/VKPipelineDataBuild.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{
PipelineData *LoadPipelineFromFile(const OSString &filename);

namespace
{
    UnorderedMap<OSString,PipelineData*> pipeline_data_by_filename;
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

PipelineData *BuildPipelineData(const mtl::MaterialPipelineConfig &config,
                                const bool double_sided,
                                const float alpha_cutoff)
{
    // 默认状态与旧 Solid3D 预设一致：cull Back、depth test/write 开、compare GreaterOrEqual、混合关
    PipelineData *pd = new PipelineData(1);

    // 剔除
    pd->SetCullMode(static_cast<VkCullModeFlagBits>(config.cull_mode));

    if(double_sided)
        pd->CloseCullFace();

    // 深度
    pd->SetDepthTest(config.depth_test);
    pd->SetDepthWrite(config.depth_write);
    pd->SetDepthCompareOp(config.depth_compare_op);

    // 混合
    if(config.alpha_blend)
    {
        pd->OpenBlend(0);
        pd->SetColorBlend(0,VK_BLEND_OP_ADD,config.blend_src,config.blend_dst);
        pd->SetAlphaBlend(0,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO);
    }

    // 线宽
    pd->SetLineWidth(config.line_width);

    if(config.dynamic_line_width)
        pd->AddDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);

    // 线框
    if(config.wireframe)
        pd->SetPolygonMode(VK_POLYGON_MODE_LINE);

    // alpha test
    if(alpha_cutoff>0.0f)
        pd->SetAlphaTest(alpha_cutoff);

    return pd;
}
}//namespace hgl::graph
