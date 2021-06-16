#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKPipelineData.h>

VK_NAMESPACE_BEGIN
Pipeline *RenderTarget::CreatePipeline(Material *mtl,const InlinePipeline &ip,const Prim &prim,const bool prim_restart)
{
    if(!mtl)return(nullptr);

    PipelineData *pd=GetPipelineData(ip);

    pd->Set(prim,prim_restart);

    Pipeline *p=device->CreatePipeline(pd,mtl,this);

    if(p)
        pipeline_list.Add(p);

    return p;
}

Pipeline *RenderTarget::CreatePipeline(MaterialParameters *mi,const InlinePipeline &ip,const Prim &prim,const bool prim_restart)
{
    if(!mi)return(nullptr);

    return CreatePipeline(mi->GetMaterial(),ip,prim,prim_restart);
}

Pipeline *RenderTarget::CreatePipeline(Material *mtl,PipelineData *pd,const Prim &prim,const bool prim_restart)
{
    pd->Set(prim,prim_restart);
    
    Pipeline *p=device->CreatePipeline(pd,mtl,this);

    if(p)
        pipeline_list.Add(p);

    return(p);
}

Pipeline *RenderTarget::CreatePipeline(MaterialParameters *mi,PipelineData *pd,const Prim &prim,const bool prim_restart)
{
    return CreatePipeline(mi->GetMaterial(),pd,prim,prim_restart);
}

Pipeline *RenderTarget::CreatePipeline(Material *mtl,const OSString &pipeline_filename,const Prim &prim,const bool prim_restart)
{
    PipelineData *pd=GetPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    return CreatePipeline(mtl,pd,prim,prim_restart);
}

Pipeline *RenderTarget::CreatePipeline(MaterialParameters *mi,const OSString &filename,const Prim &prim,const bool prim_restart)
{
    return CreatePipeline(mi->GetMaterial(),filename,prim,prim_restart);
}
VK_NAMESPACE_END