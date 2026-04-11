#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/common/VertexInputDef.h>
#include<hgl/common/VertexFormatMap.h>
#include<hgl/type/BlockAllocator.h>
#include<hgl/log/Logger.h>
#include<vector>

namespace hgl::graph{

class BufferManager;

class VertexDataManager
{
    OBJECT_LOGGER

    VulkanDevice *device;
    BufferManager *buffer_manager;

    // TODO: Migrate VAB/IBO allocations to BufferManager while keeping pooled behavior.

protected:

    VertexFormatMap vertex_format_map; ///<仅保留 VertexAttrib + VkFormat 的几何布局描述

    VkDeviceSize    vab_max_size;   ///<顶点缓冲区分配空间大小(顶点数)
    VkDeviceSize    vab_cur_size;   ///<顶点缓冲区当前使用大小
    VAB **          vab;            ///<顶点缓冲区列表

    VkDeviceSize    ibo_cur_size;   ///<索引缓冲区当前使用大小
    IndexBuffer *   ibo;            ///<索引缓冲区

protected:

    BlockAllocator       vbo_data_chain; ///<数据链
    BlockAllocator       ibo_data_chain; ///<数据链

public:

    VertexDataManager(VulkanDevice *dev,const VertexFormatMap &format_map);
    VertexDataManager(BufferManager *bm,const VertexFormatMap &format_map);
    ~VertexDataManager();

          VulkanDevice *GetDevice       ()const{return device;}                                     ///<取得GPU设备
          BufferManager *GetBufferManager()const{return buffer_manager;}                             ///<取得BufferManager

    const uint          GetVertexAttribCount()const{return static_cast<uint>(vertex_format_map.size());} ///<取得顶点属性数量(不依赖外部VIL)
    const VertexFormatMap &      GetVertexFormatMap()const{return vertex_format_map;}

    int GetVABIndex(const VertexAttrib attrib) const
    {
        int index=0;
        for(const auto &[key, _] : vertex_format_map)
        {
            if(key==attrib)
                return index;

            ++index;
        }

        return -1;
    }

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
    VAB *GetVAB(const uint index){return index<GetVertexAttribCount()?vab[index]:nullptr;}
};//class VertexDataManager

using VDM=VertexDataManager;
}//namespace hgl::graph
