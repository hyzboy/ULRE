#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>

namespace hgl::ecs
{
    MaterialBatch::MaterialBatch(const ShaderProgramPipelineKey& k, graph::VulkanDevice* dev, graph::BufferManager* bm)
        : key(k)
        , static_count(0)
        , cameraInfo(nullptr)
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
        for (size_t i = 0; i < graph::DESCRIPTOR_SET_TYPE_COUNT; ++i)
        {
            delete batch_descriptor_mp[i];
            batch_descriptor_mp[i] = nullptr;
        }
        has_batch_descriptor_overrides = false;

        if (icb_mesh_tasks)
            delete icb_mesh_tasks;
        if (mesh_draw_params_buffer)
        {
            if (buffer_manager)
                buffer_manager->Release(mesh_draw_params_buffer);
            else
                delete mesh_draw_params_buffer;
        }
        if (l2w_index_rows_buffer)
        {
            if (buffer_manager)
                buffer_manager->Release(l2w_index_rows_buffer);
            else
                delete l2w_index_rows_buffer;
        }
        if (material_data_index_rows_buffer)
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
        descriptor_bind_valid = true;

        has_batch_descriptor_overrides = false;
        for (size_t i = 0; i < graph::DESCRIPTOR_SET_TYPE_COUNT; ++i)
        {
            if (batch_descriptor_mp[i] && batch_descriptor_mp[i]->GetDescriptorSet())
                batch_descriptor_mp[i]->GetDescriptorSet()->Clear();
        }
    }

    void MaterialBatch::AddItem(RenderItem* item)
    {
        if (!item)
            return;

        item->index = static_cast<uint32_t>(items.size());
        items.push_back(item);
    }

}//namespace hgl::ecs
