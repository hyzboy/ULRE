#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/type/DataChain.h>
#include<hgl/log/Logger.h>

VK_NAMESPACE_BEGIN

class VertexDataManager
{
    OBJECT_LOGGER

    VulkanDevice *device;

protected:

    const VIL *     vil;            ///<顶点输入格式列表
          uint      vi_count;       ///<顶点输入流数量
    const VIF *     vif_list;       ///<顶点输入格式列表

    VkDeviceSize    vab_max_size;   ///<顶点缓冲区分配空间大小(顶点数)
    VkDeviceSize    vab_cur_size;   ///<顶点缓冲区当前使用大小
    VAB **          vab;            ///<顶点缓冲区列表

    VkDeviceSize    ibo_cur_size;   ///<索引缓冲区当前使用大小
    IndexBuffer *   ibo;            ///<索引缓冲区

protected:

    DataChain       vbo_data_chain; ///<数据链
    DataChain       ibo_data_chain; ///<数据链

public:

    VertexDataManager(VulkanDevice *dev,const VIL *_vil);
    ~VertexDataManager();

          VulkanDevice *GetDevice       ()const{return device;}                                     ///<取得GPU设备

    const VIL *         GetVIL          ()const{return vil;}                                         ///<取得顶点输入格式列表

    const VkDeviceSize  GetVABMaxCount  ()const{return vab_max_size;}                                ///<取得顶点属性缓冲区分配的空间最大数量
    const VkDeviceSize  GetVABCurCount  ()const{return vab_cur_size;}                                ///<取得顶点属性缓冲区当前数量

    const IndexType     GetIndexType      ()const{return ibo?ibo->GetType():IndexType::ERR;}         ///<取得索引缓冲区类型
    const VkDeviceSize  GetIndexMaxCount  ()const{return ibo?ibo->GetCount():-1;}                    ///<取得索引缓冲区分配的空间最大数量
    const VkDeviceSize  GetIndexCurCount  ()const{return ibo?ibo_cur_size:-1;}                       ///<取得索引缓冲区当前数量

public:

    bool Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type);

    DataChain::UserNode *AcquireIB(const VkDeviceSize count);
    DataChain::UserNode *AcquireVAB(const VkDeviceSize count);

    bool ReleaseIB(DataChain::UserNode *);
    bool ReleaseVAB(DataChain::UserNode *);

    IndexBuffer *GetIBO(){return ibo;}
    VAB *GetVAB(const uint index){return vab[index];}
};//class VertexDataManager

using VDM=VertexDataManager;
VK_NAMESPACE_END
