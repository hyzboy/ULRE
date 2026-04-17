#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>

namespace hgl::ecs
{
    MaterialBatch::MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev, graph::BufferManager* bm)
        : key(k)
        , static_count(0)
        , cameraInfo(nullptr)
        , device(dev)
        , buffer_manager(bm)
        , draw_batches_count(0)
        , renderer(nullptr)
    {
        if (key.material && key.pipeline)
        {
            // Create ECS renderer
            renderer = new PipelineMaterialRenderer(key.material, key.pipeline);
        }
    }

    MaterialBatch::~MaterialBatch()
    {
        if (icb_draw_indexed)
            delete icb_draw_indexed;
        if (icb_draw)
            delete icb_draw;
        if (transform_buffer)
            delete transform_buffer;
        if (mi_buffer)
            delete mi_buffer;
        if (renderer)
            delete renderer;
    }

    void MaterialBatch::AddItem(RenderItem* item)
    {
        if (!item)
            return;

        item->index = static_cast<uint32_t>(items.size());
        items.push_back(item);
    }

}//namespace hgl::ecs

