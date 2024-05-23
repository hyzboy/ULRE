#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VDMAccess.h>

VK_NAMESPACE_BEGIN

class VertexDataManager
{
    GPUDevice *device;

protected:

    const VIL *     vil;            ///<顶点输入格式列表
          uint      vi_count;       ///<顶点输入流数量
    const VIF *     vif_list;       ///<顶点输入格式列表

    VkDeviceSize    vab_max_size;   ///<顶点缓冲区分配空间大小
    VkDeviceSize    vab_cur_size;   ///<顶点缓冲区当前使用大小
    VAB **          vab;            ///<顶点缓冲区列表

    VkDeviceSize    ibo_cur_size;   ///<索引缓冲区当前使用大小
    IndexBuffer *   ibo;            ///<索引缓冲区

protected:

    DataChain       vbo_data_chain; ///<数据链
    DataChain       ibo_data_chain; ///<数据链

protected:

    friend struct IBAccessVDM;
    friend struct VABAccessVDM;

    bool ReleaseIB(DataChain::UserNode *);
    bool ReleaseVAB(DataChain::UserNode *);

public:

    VertexDataManager(GPUDevice *dev,const VIL *_vil);
    ~VertexDataManager();

            GPUDevice *   GetDevice       ()const{return device;}                                     ///<取得GPU设备

    const VIL *         GetVIL          ()const{return vil;}                                         ///<取得顶点输入格式列表

    const VkDeviceSize  GetVABMaxCount  ()const{return vab_max_size;}                                ///<取得顶点属性缓冲区分配的空间最大数量
    const VkDeviceSize  GetVABCurCount  ()const{return vab_cur_size;}                                ///<取得顶点属性缓冲区当前数量

    const IndexType     GetIBOType      ()const{return ibo?ibo->GetType():IndexType::ERR;}           ///<取得索引缓冲区类型
    const VkDeviceSize  GetIBOMaxCount  ()const{return ibo?ibo->GetCount():-1;}                      ///<取得索引缓冲区分配的空间最大数量
    const VkDeviceSize  GetIBOCurCount  ()const{return ibo?ibo_cur_size:-1;}                         ///<取得索引缓冲区当前数量

public:

    bool Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type);

    IBAccessVDM *AcquireIB(const VkDeviceSize count);
    VABAccessVDM *AcquireVAB(const VkDeviceSize count);

    void Release(VABAccessVDM *);
    void Release(IBAccessVDM *);
};//class VertexDataManager
VK_NAMESPACE_END
