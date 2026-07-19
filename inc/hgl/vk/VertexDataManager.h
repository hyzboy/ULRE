#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/type/BlockAllocator.h>
#include<hgl/log/Logger.h>

namespace hgl::graph{

class BufferManager;

class VertexDataManager
{
    OBJECT_LOGGER

    VulkanDevice *device;
    BufferManager *buffer_manager;

    // TODO: Migrate VAB/IBO allocations to BufferManager while keeping pooled behavior.

protected:

    GeometryVertexFormat geometry_vertex_format;
          uint      vi_count;       ///<顶点输入流数量

    VkDeviceSize    vab_max_size;   ///<顶点缓冲区分配空间大小(顶点数)
    VkDeviceSize    vab_cur_size;   ///<顶点缓冲区当前使用大小
    VAB **          vab;            ///<顶点缓冲区列表

    VkDeviceSize    ibo_cur_size;   ///<索引缓冲区当前使用大小
    IndexBuffer *   ibo;            ///<索引缓冲区

protected:

    BlockAllocator       vbo_data_chain; ///<数据链
    BlockAllocator       ibo_data_chain; ///<数据链

public:

    VertexDataManager(VulkanDevice *dev,const GeometryVertexFormat &gvf);
    VertexDataManager(BufferManager *bm,const GeometryVertexFormat &gvf);
    ~VertexDataManager();

          VulkanDevice *GetDevice       ()const{return device;}                                     ///<取得GPU设备
          BufferManager *GetBufferManager()const{return buffer_manager;}                             ///<取得BufferManager

    const GeometryVertexFormat &GetGeometryVertexFormat()const{return geometry_vertex_format;}

    const VkDeviceSize  GetVABMaxCount  ()const{return vab_max_size;}                                ///<取得顶点属性缓冲区分配的空间最大数量
    const VkDeviceSize  GetVABCurCount  ()const{return vab_cur_size;}                                ///<取得顶点属性缓冲区当前数量

    const IndexType     GetIndexType      ()const{return ibo?ibo->GetType():IndexType::ERR;}         ///<取得索引缓冲区类型
    const VkDeviceSize  GetIndexMaxCount  ()const{return ibo?ibo->GetCount():-1;}                    ///<取得索引缓冲区分配的空间最大数量
    const VkDeviceSize  GetIndexCurCount  ()const{return ibo?ibo_cur_size:-1;}                       ///<取得索引缓冲区当前数量

public:

    bool Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type);

    BlockAllocator::UserNode *AcquireIB(const VkDeviceSize count);
    BlockAllocator::UserNode *AcquireVAB(const VkDeviceSize count);

    bool ReleaseIB(BlockAllocator::UserNode *);
    bool ReleaseVAB(BlockAllocator::UserNode *);

    IndexBuffer *GetIBO(){return ibo;}
    VAB *GetVAB(const uint index){return vab[index];}
};//class VertexDataManager

using VDM=VertexDataManager;
}//namespace hgl::graph
