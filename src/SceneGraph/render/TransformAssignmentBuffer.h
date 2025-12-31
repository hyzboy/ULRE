#pragma once
#include<hgl/graph/DrawNode.h>

VK_NAMESPACE_BEGIN

/*
* 渲染节点Transform数据管理
*/
class TransformAssignmentBuffer
{
private:

    uint MaxTransformCount;

    VulkanDevice *device;

private:    //LocalToWorld矩阵数据

    uint32 transform_buffer_max_count;        ///<LocalToWorld矩阵最大数量
    DeviceBuffer *transform_buffer;           ///<LocalToWorld矩阵数据(UBO/SSBO)

    void StatTransform(const DrawNodeList &);

private:    //分发数据

    uint32 node_count;                  ///<节点数量

    VAB *transform_vab;                 ///<LocalToWorld矩阵ID分发数据VAB(R16UI格式)
    VkBuffer transform_vab_buffer;      ///<LocalToWorld矩阵ID分发数据Buffer

private:

    void Clear();

public:

    const VkBuffer GetTransformVAB()const{return transform_vab_buffer;}

    void BindTransform(Material *)const;

public:

    TransformAssignmentBuffer(VulkanDevice *dev);
    ~TransformAssignmentBuffer(){Clear();}

    void WriteNode(const DrawNodeList &);

    void UpdateTransformData(const DrawNodeList &,const int first,const int last);
};//class TransformAssignmentBuffer
VK_NAMESPACE_END
