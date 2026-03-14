#include<hgl/graph/VertexDataBufferManager.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKStagedBuffer.h>

namespace hgl::graph
{
    VertexDataBufferManager::VertexDataBufferManager(VulkanDevice *dev)
        :device(dev),
         vertex_buffer(nullptr),
         index_buffer(nullptr),
         max_vertices(0),
         max_indices(0)
    {
    }

    VertexDataBufferManager::~VertexDataBufferManager()
    {
        delete vertex_buffer;
        delete index_buffer;
    }

    bool VertexDataBufferManager::Init(uint32_t max_vertex_count,uint32_t max_index_count)
    {
        if(!device||max_vertex_count==0||max_index_count==0)
            return false;

        const VkDeviceSize vertex_size=(VkDeviceSize)max_vertex_count*sizeof(SSBOVertexData);
        const VkDeviceSize index_size =(VkDeviceSize)max_index_count*sizeof(uint32_t);

        vertex_buffer=device->CreateStagedBuffer(
            ObjectNameBuilder("SSBOVertexData"),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            vertex_size);

        if(!vertex_buffer)
            return false;

        index_buffer=device->CreateStagedBuffer(
            ObjectNameBuilder("SSBOIndexData"),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            index_size);

        if(!index_buffer)
        {
            delete vertex_buffer;
            vertex_buffer=nullptr;
            return false;
        }

        if(!vertex_allocator.Init(max_vertex_count))
        {
            delete vertex_buffer;
            delete index_buffer;
            vertex_buffer=nullptr;
            index_buffer=nullptr;
            return false;
        }

        if(!index_allocator.Init(max_index_count))
        {
            delete vertex_buffer;
            delete index_buffer;
            vertex_buffer=nullptr;
            index_buffer=nullptr;
            return false;
        }

        max_vertices=max_vertex_count;
        max_indices =max_index_count;
        return true;
    }

    //-- Vertex block operations -----------------------------------------------

    BlockAllocator::UserNode *VertexDataBufferManager::AllocateVertexBlock(uint32_t vertex_count)
    {
        if(vertex_count==0)
            return nullptr;

        return vertex_allocator.Acquire(vertex_count);
    }

    bool VertexDataBufferManager::UploadVertices(const BlockAllocator::UserNode *node,const SSBOVertexData *data,uint32_t count)
    {
        if(!node||!data||!vertex_buffer||count==0)
            return false;

        if((uint32_t)node->GetCount()<count)
            return false;

        const VkDeviceSize offset=(VkDeviceSize)node->GetStart()*sizeof(SSBOVertexData);
        const VkDeviceSize size  =(VkDeviceSize)count*sizeof(SSBOVertexData);

        return vertex_buffer->Write(data,offset,size);
    }

    bool VertexDataBufferManager::FreeVertexBlock(BlockAllocator::UserNode *node)
    {
        if(!node)
            return false;

        return vertex_allocator.Release(node);
    }

    //-- Index block operations ------------------------------------------------

    BlockAllocator::UserNode *VertexDataBufferManager::AllocateIndexBlock(uint32_t index_count)
    {
        if(index_count==0)
            return nullptr;

        return index_allocator.Acquire(index_count);
    }

    bool VertexDataBufferManager::UploadIndices(const BlockAllocator::UserNode *node,const uint32_t *data,uint32_t count)
    {
        if(!node||!data||!index_buffer||count==0)
            return false;

        if((uint32_t)node->GetCount()<count)
            return false;

        const VkDeviceSize offset=(VkDeviceSize)node->GetStart()*sizeof(uint32_t);
        const VkDeviceSize size  =(VkDeviceSize)count*sizeof(uint32_t);

        return index_buffer->Write(data,offset,size);
    }

    bool VertexDataBufferManager::FreeIndexBlock(BlockAllocator::UserNode *node)
    {
        if(!node)
            return false;

        return index_allocator.Release(node);
    }

    //-- Descriptor info -------------------------------------------------------

    VkDescriptorBufferInfo VertexDataBufferManager::GetVertexDescriptorInfo()const
    {
        if(vertex_buffer)
            return vertex_buffer->GetDescriptorBufferInfo();

        return {VK_NULL_HANDLE,0,0};
    }

    VkDescriptorBufferInfo VertexDataBufferManager::GetIndexDescriptorInfo()const
    {
        if(index_buffer)
            return index_buffer->GetDescriptorBufferInfo();

        return {VK_NULL_HANDLE,0,0};
    }

    //-- Dirty tracking / GPU transfer -----------------------------------------

    bool VertexDataBufferManager::IsDirty()const
    {
        return (vertex_buffer&&vertex_buffer->IsDirty())
             ||(index_buffer &&index_buffer->IsDirty());
    }

    void VertexDataBufferManager::CopyToDevice(VkCommandBuffer cmd)
    {
        if(vertex_buffer&&vertex_buffer->IsDirty())
            vertex_buffer->CopyToDevice(cmd);

        if(index_buffer&&index_buffer->IsDirty())
            index_buffer->CopyToDevice(cmd);
    }

    //-- Utility: upload geometry to SSBO --------------------------------------

    bool UploadGeometryToSSBO(Geometry *geo,
                              VertexDataBufferManager *vdbm,
                              const SSBOVertexData *vertices,uint32_t vertex_count,
                              const uint32_t *indices,uint32_t index_count)
    {
        if(!geo||!vdbm||!vertices||vertex_count==0)
            return false;

        auto *vtx_node=vdbm->AllocateVertexBlock(vertex_count);
        if(!vtx_node)
            return false;

        if(!vdbm->UploadVertices(vtx_node,vertices,vertex_count))
        {
            vdbm->FreeVertexBlock(vtx_node);
            return false;
        }

        geo->SetSSBOVertexNode(vtx_node);

        if(indices&&index_count>0)
        {
            auto *idx_node=vdbm->AllocateIndexBlock(index_count);
            if(!idx_node)
                return false;

            if(!vdbm->UploadIndices(idx_node,indices,index_count))
            {
                vdbm->FreeIndexBlock(idx_node);
                return false;
            }

            geo->SetSSBOIndexNode(idx_node);
        }

        return true;
    }
}//namespace hgl::graph
