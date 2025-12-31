#include<hgl/ecs/MaterialBatch.h>
#include<hgl/ecs/RenderItem.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/mesh/Primitive.h>
#include<algorithm>

namespace hgl::ecs
{
    MaterialBatch::MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev)
        : key(k)
        , cameraInfo(nullptr)
        , device(dev)
    {
    }

    void MaterialBatch::AddItem(RenderItem* item)
    {
        if (item)
            items.push_back(item);
    }

    void MaterialBatch::Finalize()
    {
        // Sort items by geometry/distance for optimal rendering
        std::sort(items.begin(), items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });
    }

    void MaterialBatch::Render(graph::RenderCmdBuffer* cmdBuffer)
    {
        if (!cmdBuffer || items.empty())
            return;

        if (!key.material || !key.pipeline)
            return;

        // Bind pipeline
        cmdBuffer->BindPipeline(key.pipeline);

        // Bind material descriptor sets
        cmdBuffer->BindDescriptorSets(key.material);

        // Render each item
        for (RenderItem* item : items)
        {
            if (!item || !item->isVisible)
                continue;

            auto* primitive = item->GetPrimitive();
            if (!primitive)
                continue;

            // Get geometry data
            auto* dataBuffer = primitive->GetDataBuffer();
            auto* drawRange = primitive->GetRenderData();

            if (!dataBuffer || !drawRange)
                continue;

            // Bind vertex buffers (VAB)
            // Note: This is simplified - the real implementation would use VABList
            // cmdBuffer->BindVAB(dataBuffer);

            // Bind index buffer if present
            if (dataBuffer->ibo)
                cmdBuffer->BindIBO(dataBuffer->ibo);

            // Draw the primitive
            // Instance count is 1 for non-instanced rendering
            // first_instance corresponds to the transform index
            cmdBuffer->Draw(dataBuffer, drawRange, 1, item->transform_index);
        }
    }
}//namespace hgl::ecs
