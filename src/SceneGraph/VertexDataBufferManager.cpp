#include<hgl/graph/VertexDataBufferManager.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKStagedBuffer.h>
#include<cstring>
#include<vector>

namespace hgl::graph
{
    namespace
    {
        static uint32_t GetIndexTypeByteStride(const IndexType index_type)
        {
            if(index_type==IndexType::U8) return 1;
            if(index_type==IndexType::U16) return 2;
            if(index_type==IndexType::U32) return 4;
            return 0;
        }

        static bool ValidateMeshIndexUploadInfo(const MeshIndexStreamUploadInfo &info,
                                                uint32_t &type_stride,
                                                uint32_t &source_stride,
                                                VkDeviceSize &stream_begin)
        {
            if(!info.source_data || info.source_size==0 || info.index_count==0)
                return false;

            type_stride=GetIndexTypeByteStride(info.source_index_type);
            if(type_stride==0)
                return false;

            source_stride=(info.source_stride==0)?type_stride:info.source_stride;
            if(source_stride<type_stride)
                return false;

            if((info.source_offset%type_stride)!=0 || (source_stride%type_stride)!=0)
                return false;

            if(info.source_offset>=info.source_size)
                return false;

            stream_begin=info.source_offset+VkDeviceSize(info.first_index)*source_stride;
            if(stream_begin>=info.source_size)
                return false;

            const VkDeviceSize last_entry=stream_begin+VkDeviceSize(info.index_count-1)*source_stride;
            const VkDeviceSize required_end=last_entry+type_stride;

            return required_end<=info.source_size;
        }
    }

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
        return UploadIndices(node,
                             static_cast<const void *>(data),
                             count,
                             IndexType::U32,
                             uint32_t(sizeof(uint32_t)));
    }

    bool VertexDataBufferManager::UploadIndices(const BlockAllocator::UserNode *node,
                                                const void *data,
                                                uint32_t count,
                                                IndexType source_index_type,
                                                uint32_t source_stride)
    {
        const uint32_t type_stride=GetIndexTypeByteStride(source_index_type);
        if(type_stride==0)
            return false;

        MeshIndexStreamUploadInfo info;
        info.source_data=data;
        info.source_stride=source_stride;
        info.source_index_type=source_index_type;
        info.first_index=0;
        info.index_count=count;
        info.source_size=VkDeviceSize(count)*(source_stride==0?type_stride:source_stride);

        return UploadIndexStream(node,info);
    }

    bool VertexDataBufferManager::UploadIndexStream(const BlockAllocator::UserNode *node,const MeshIndexStreamUploadInfo &info)
    {
        if(!node||!index_buffer)
            return false;

        if((uint32_t)node->GetCount()<info.index_count)
            return false;

        uint32_t type_stride=0;
        uint32_t source_stride=0;
        VkDeviceSize stream_begin=0;
        if(!ValidateMeshIndexUploadInfo(info,type_stride,source_stride,stream_begin))
            return false;

        std::vector<uint32_t> converted_indices;
        converted_indices.resize(info.index_count);

        const uint8_t *stream_bytes=static_cast<const uint8_t *>(info.source_data);

        for(uint32_t i=0;i<info.index_count;i++)
        {
            const VkDeviceSize entry_offset=stream_begin+VkDeviceSize(i)*source_stride;
            const uint8_t *entry_ptr=stream_bytes+entry_offset;

            uint32_t value=0;
            if(type_stride==1)
            {
                value=*entry_ptr;
            }
            else if(type_stride==2)
            {
                uint16_t u16=0;
                std::memcpy(&u16,entry_ptr,sizeof(uint16_t));
                value=u16;
            }
            else if(type_stride==4)
            {
                std::memcpy(&value,entry_ptr,sizeof(uint32_t));
            }
            else
            {
                return false;
            }

            converted_indices[i]=value;
        }

        const VkDeviceSize offset=VkDeviceSize(node->GetStart())*sizeof(uint32_t);
        const VkDeviceSize size=VkDeviceSize(info.index_count)*sizeof(uint32_t);

        return index_buffer->Write(converted_indices.data(),offset,size);
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
            MeshIndexStreamUploadInfo info;
            info.source_data=indices;
            info.source_size=VkDeviceSize(index_count)*sizeof(uint32_t);
            info.source_offset=0;
            info.source_stride=sizeof(uint32_t);
            info.source_index_type=IndexType::U32;
            info.first_index=0;
            info.index_count=index_count;

            if(!UploadGeometryIndexStreamToSSBO(geo,vdbm,info))
                return false;
        }

        return true;
    }

    bool UploadGeometryIndexStreamToSSBO(Geometry *geo,
                                         VertexDataBufferManager *vdbm,
                                         const MeshIndexStreamUploadInfo &info)
    {
        if(!geo||!vdbm||info.index_count==0)
            return false;

        auto *idx_node=vdbm->AllocateIndexBlock(info.index_count);
        if(!idx_node)
            return false;

        if(!vdbm->UploadIndexStream(idx_node,info))
        {
            vdbm->FreeIndexBlock(idx_node);
            return false;
        }

        geo->SetSSBOIndexNode(idx_node);
        return true;
    }
}//namespace hgl::graph
