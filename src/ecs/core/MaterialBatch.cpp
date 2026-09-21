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
        , static_count(0)
        , device(dev)
        , buffer_manager(bm)
        , draw_batches_count(0)
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
        if (own_icb_mesh_tasks && icb_mesh_tasks)
            delete icb_mesh_tasks;
        if (own_mesh_draw_params && mesh_draw_params_buffer)
        {
            if (buffer_manager)
                buffer_manager->Release(mesh_draw_params_buffer);
            else
                delete mesh_draw_params_buffer;
        }
        if (own_l2w_index && l2w_index_buffer)
        {
            if (buffer_manager)
                buffer_manager->Release(l2w_index_buffer);
            else
                delete l2w_index_buffer;
        }
        if (own_material_data_rows && material_data_index_rows_buffer)
        {
            if (buffer_manager)
                buffer_manager->Release(material_data_index_rows_buffer);
            else
                delete material_data_index_rows_buffer;
        }
        if (renderer)
            delete renderer;
    }

    void MaterialBatch::Clear()
    {
        items.clear();
        static_count = 0;
        draw_batches.clear();
        draw_batches_count = 0;
        texture_reference_base_addr = 0;
        icb_count_buffer = nullptr;
        icb_count_buffer_offset = 0;

        if (!own_icb_mesh_tasks)
            icb_mesh_tasks = nullptr;
        if (!own_mesh_draw_params)
            mesh_draw_params_buffer = nullptr;
        if (!own_l2w_index)
            l2w_index_buffer = nullptr;
        if (!own_material_data_rows)
            material_data_index_rows_buffer = nullptr;

        l2w_buffer = nullptr;
        gpu_driven_override = false;
        uses_render_item_resolve = false;
        own_icb_mesh_tasks = true;
        own_mesh_draw_params = true;
        own_l2w_index = true;
        own_material_data_rows = true;
    }

    void MaterialBatch::AddItem(RenderItem* item)
    {
        if (!item)
            return;

        item->index = static_cast<uint32_t>(items.size());
        items.push_back(item);
    }

}//namespace hgl::ecs
