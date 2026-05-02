#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/BlockAllocator.h>

namespace hgl::graph
{
    class VulkanDevice;
    class StagedBuffer;

    struct MeshIndexStreamUploadInfo
    {
        const void *source_data = nullptr;     ///< Source index stream base pointer
        VkDeviceSize source_size = 0;          ///< Total source byte size available
        VkDeviceSize source_offset = 0;        ///< Byte offset from source_data to stream start
        uint32_t source_stride = 0;            ///< Byte stride between indices (0 = tightly packed)
        IndexType source_index_type = IndexType::U32;
        uint32_t first_index = 0;              ///< First index element within the source stream
        uint32_t index_count = 0;              ///< Number of indices to upload
    };

    /**
     * GPU VertexData 结构匹配 GLSL std430 layout:
     *   struct VertexData { vec3 position; vec3 normal; vec2 uv0; };
     */
    struct SSBOVertexData
    {
        float position[3];     // offset  0, size 12
        float _pad0;           // offset 12, size  4 (vec3 alignment)
        float normal[3];       // offset 16, size 12
        float _pad1;           // offset 28, size  4 (vec3 alignment)
        float uv0[2];          // offset 32, size  8
        float _pad2[2];        // offset 40, size  8 (struct alignment to 16)
    };

    static_assert(sizeof(SSBOVertexData)==48,"Must match GLSL std430 VertexData layout");

    /**
     * 全局 SSBO 顶点/索引缓冲区管理器。
     * 管理一个大的 VkBuffer(SSBO)存储所有 mesh 的顶点数据，
     * 以及一个大的 VkBuffer(SSBO)存储所有 mesh 的索引数据。
     * 用于 SSBO vertex fetch 路径（PC / High-quality 平台）。
     */
    class VertexDataBufferManager
    {
        VulkanDevice *device;

        StagedBuffer *vertex_buffer;
        StagedBuffer *index_buffer;

        BlockAllocator vertex_allocator;
        BlockAllocator index_allocator;

        uint32_t max_vertices;
        uint32_t max_indices;

    public:

        VertexDataBufferManager(VulkanDevice *dev);
        ~VertexDataBufferManager();

        bool Init(uint32_t max_vertex_count,uint32_t max_index_count);

    public: // Vertex block operations

        BlockAllocator::UserNode *AllocateVertexBlock(uint32_t vertex_count);
        bool UploadVertices(const BlockAllocator::UserNode *node,const SSBOVertexData *data,uint32_t count);
        bool FreeVertexBlock(BlockAllocator::UserNode *node);

    public: // Index block operations

        BlockAllocator::UserNode *AllocateIndexBlock(uint32_t index_count);
        bool UploadIndices(const BlockAllocator::UserNode *node,const uint32_t *data,uint32_t count);
        bool UploadIndices(const BlockAllocator::UserNode *node,
                           const void *data,
                           uint32_t count,
                           IndexType source_index_type,
                           uint32_t source_stride=0);
        bool UploadIndexStream(const BlockAllocator::UserNode *node,const MeshIndexStreamUploadInfo &info);
        bool FreeIndexBlock(BlockAllocator::UserNode *node);

    public: // Buffer access for descriptor binding

        StagedBuffer *GetVertexBuffer()const{return vertex_buffer;}
        StagedBuffer *GetIndexBuffer()const{return index_buffer;}

        VkDescriptorBufferInfo GetVertexDescriptorInfo()const;
        VkDescriptorBufferInfo GetIndexDescriptorInfo()const;

    public: // Dirty tracking and GPU transfer

        bool IsDirty()const;
        void CopyToDevice(VkCommandBuffer cmd);
    };//class VertexDataBufferManager

    class Geometry;

    /**
     * 将几何体数据上传到全局 SSBO 并记录分配信息到 Geometry
     * @return true 如果分配和上传都成功
     */
    bool UploadGeometryToSSBO(Geometry *geo,
                              VertexDataBufferManager *vdbm,
                              const SSBOVertexData *vertices,uint32_t vertex_count,
                              const uint32_t *indices=nullptr,uint32_t index_count=0);

    /**
     * Upload mesh index stream into the global SSBO index buffer with
     * stride/offset/range validation and set Geometry SSBO index node.
     */
    bool UploadGeometryIndexStreamToSSBO(Geometry *geo,
                                         VertexDataBufferManager *vdbm,
                                         const MeshIndexStreamUploadInfo &info);
}//namespace hgl::graph
