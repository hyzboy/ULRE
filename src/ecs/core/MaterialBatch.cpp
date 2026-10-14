#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/IndirectCommandBuffer.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>

namespace hgl::ecs
{
    MaterialBatch::MaterialBatch(const ShaderProgramPipelineKey& k, graph::VulkanDevice* dev, graph::BufferManager* bm)
        : key(k)
        , device(dev)
        , buffer_manager(bm)
        , renderer(nullptr)
    {
        if (key.shader_program && key.pipeline)
        {
            // Create ECS renderer
            renderer = new PipelineMaterialRenderer(key.shader_program, key.pipeline);
        }
    }

    MaterialBatch::~MaterialBatch()
    {
        // 2026-10-14（批 D）：CPU 侧 per-batch 表构建已整删 ⇒ 本批只持**非拥有**指针（指向剔除输出）
        // ⇒ 析构不再释放任何 per-batch 表（GPU 输出由 GPUCullSystem 拥有）。
        if (renderer)
            delete renderer;
    }

    void MaterialBatch::Clear()
    {
        items.clear();
        texture_reference_base_addr = 0;
        icb_count_buffer = nullptr;
        // T4c3c：计数偏移随切片按 pass 存（`gpu_slice[p].count_buffer_offset`）

        // 2026-10-14（批 D）：四个 per-batch 表指针一律**非拥有** ⇒ 复位无条件置空（不再有 own_* 分支）。
        icb_mesh_tasks                    = nullptr;
        mesh_draw_params_buffer           = nullptr;
        l2w_index_buffer                  = nullptr;
        material_data_index_rows_buffer   = nullptr;
        gpu_driven_override = false;
        for (uint32_t p = 0; p < RenderPassSlot::Count; ++p)   // T4c3c：切片随 override 作废（两 pass 同清）
        {
            gpu_slice[p].row_base            = 0;
            gpu_slice[p].capacity            = 0;
            gpu_slice[p].count_buffer_offset = 0;
        }
    }

    void MaterialBatch::AddItem(RenderItem* item)
    {
        if (!item)
            return;

        items.push_back(item);
    }

}//namespace hgl::ecs
