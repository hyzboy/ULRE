#include<hgl/graph/InstancedMeshRenderer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/component/InstancedMeshComponent.h>

VK_NAMESPACE_BEGIN

InstancedMeshRenderer::InstancedMeshRenderer(VulkanDevice* d)
{
    device = d;
    cmd_buf = nullptr;
    camera_info = nullptr;

    icb_draw = nullptr;
    icb_draw_indexed = nullptr;

    vab_list = nullptr;

    last_data_buffer = nullptr;
    last_vdm = nullptr;
    last_render_data = nullptr;
}

InstancedMeshRenderer::~InstancedMeshRenderer()
{
    CleanupResources();
}

void InstancedMeshRenderer::CleanupResources()
{
    if (icb_draw) {
        delete icb_draw;
        icb_draw = nullptr;
    }
    if (icb_draw_indexed) {
        delete icb_draw_indexed;
        icb_draw_indexed = nullptr;
    }
    if (vab_list) {
        delete vab_list;
        vab_list = nullptr;
    }
}

static uint32_t power_to_2(uint32_t value)
{
    if (value <= 1) return 1;
    uint32_t result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

void InstancedMeshRenderer::ReallocICB(uint32_t count)
{
    if (count == 0)
        return;

    const uint32_t icb_new_count = power_to_2(count);

    if (icb_draw)
    {
        if (icb_new_count <= icb_draw->GetMaxCount())
            return;

        delete icb_draw;
        icb_draw = nullptr;

        delete icb_draw_indexed;
        icb_draw_indexed = nullptr;
    }

    icb_draw = device->CreateIndirectDrawBuffer(icb_new_count);
    icb_draw_indexed = device->CreateIndirectDrawIndexedBuffer(icb_new_count);
}

void InstancedMeshRenderer::WriteICB(VkDrawIndirectCommand* dicp, const InstancedMeshComponent* comp)
{
    const Mesh* mesh = comp->GetMesh();
    if (!mesh)
        return;

    const MeshRenderData* prd = mesh->GetRenderData();
    
    dicp->vertexCount = prd->vertex_count;
    dicp->instanceCount = comp->GetInstanceCount();
    dicp->firstVertex = prd->vertex_offset;
    dicp->firstInstance = 0;
}

void InstancedMeshRenderer::WriteICB(VkDrawIndexedIndirectCommand* diicp, const InstancedMeshComponent* comp)
{
    const Mesh* mesh = comp->GetMesh();
    if (!mesh)
        return;

    const MeshRenderData* prd = mesh->GetRenderData();

    diicp->indexCount = prd->index_count;
    diicp->instanceCount = comp->GetInstanceCount();
    diicp->firstIndex = prd->first_index;
    diicp->vertexOffset = prd->vertex_offset;
    diicp->firstInstance = 0;
}

bool InstancedMeshRenderer::BindVAB(const InstancedMeshComponent* comp)
{
    const Mesh* mesh = comp->GetMesh();
    if (!mesh)
        return false;

    const MeshDataBuffer* pdb = mesh->GetDataBuffer();
    const VDM* vdm = pdb->GetVDM();

    // 检查是否需要重新绑定数据缓冲区
    if (last_data_buffer != pdb || last_vdm != vdm)
    {
        if (!vab_list)
        {
            Material* material = comp->GetMaterial();
            if (!material)
                return false;
            
            vab_list = new VABList(material->GetVertexInput()->GetCount());
        }
        else
        {
            vab_list->Clear();
        }

        // 绑定顶点和索引缓冲区
        if (!vab_list->Add(pdb->GetVAB(), 0))
            return false;

        last_data_buffer = pdb;
        last_vdm = vdm;
    }

    // 绑定实例数据VAB (LocalToWorld/MaterialInstance数据)
    hgl::graph::VAB* instance_vab = comp->GetInstanceVAB();
    if (instance_vab)
    {
        if (!vab_list->Add(instance_vab, 0))
        {
            // 一般出现这个情况是因为材质中没有配置需要Instance数据
            return false;
        }
    }

    return true;
}

bool InstancedMeshRenderer::Render(hgl::graph::InstancedMeshComponent* comp, RenderCmdBuffer* rcb)
{
    if (!comp || !rcb || !comp->CanRender())
        return false;

    cmd_buf = rcb;

    // 更新实例数据缓冲区
    if (!comp->UpdateBuffers())
        return false;

    // 绑定VAB
    if (!BindVAB(comp))
        return false;

    const Mesh* mesh = comp->GetMesh();
    const MeshRenderData* prd = mesh->GetRenderData();

    // 绑定管线
    Pipeline* pipeline = comp->GetPipeline();
    if (!pipeline)
        return false;

    cmd_buf->BindPipeline(pipeline);

    // 绑定描述符集 (材质相关数据)
    Material* material = comp->GetMaterial();
    if (material)
    {
        // 这里需要绑定材质相关的描述符集
        // 具体实现取决于材质系统的接口
    }

    // 绑定顶点缓冲区
    vab_list->Bind(*cmd_buf);

    // 执行绘制
    uint32_t instance_count = comp->GetInstanceCount();
    
    if (prd->index_count > 0)
    {
        // 索引绘制
        cmd_buf->DrawIndexed(prd->index_count, instance_count, prd->first_index, prd->vertex_offset, 0);
    }
    else
    {
        // 顶点绘制
        cmd_buf->Draw(prd->vertex_count, instance_count, prd->vertex_offset, 0);
    }

    return true;
}

void InstancedMeshRenderer::RenderBatch(const ArrayList<hgl::graph::InstancedMeshComponent*>& components, RenderCmdBuffer* rcb)
{
    if (components.GetCount() == 0 || !rcb)
        return;

    cmd_buf = rcb;

    // 简单实现：逐个渲染
    // 更优化的实现可以按材质分组，使用间接绘制等
    for (uint32_t i = 0; i < components.GetCount(); i++)
    {
        Render(components[i], rcb);
    }
}

VK_NAMESPACE_END