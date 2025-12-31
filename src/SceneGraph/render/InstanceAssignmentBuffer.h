#pragma once
#include<hgl/graph/DrawNode.h>

VK_NAMESPACE_BEGIN

class MaterialInstance;

/*
* 渲染节点额外提供的数据
*/
class InstanceAssignmentBuffer
{
    uint MaxTransformCount;

private:

    VulkanDevice *device;

    Material *material;

private:    //LocalToWorld矩阵数据

    uint32 transform_buffer_max_count;        ///<LocalToWorld矩阵最大数量
    DeviceBuffer *transform_buffer;           ///<LocalToWorld矩阵数据(UBO/SSBO)

    void StatTransform(const DrawNodeList &);

private:    //材质实例数据
    
    MaterialInstanceSets mi_set;

    uint32_t        material_instance_data_bytes;           ///<单个材质实例数据字节数
    DeviceBuffer *  material_instance_buffer;               ///<材质实例数据(UBO/SSBO)

    void StatMaterialInstance(const DrawNodeList &);
    
private:    //分发数据

    uint32 node_count;                  ///<节点数量

    VAB *transform_vab;                 ///<LocalToWorld矩阵ID分发数据VAB(R16UI格式)
    VkBuffer transform_vab_buffer;      ///<LocalToWorld矩阵ID分发数据Buffer

    VAB *material_instance_vab;         ///<材质实例ID分发数据VAB(R16UI格式)
    VkBuffer material_instance_vab_buffer;  ///<材质实例ID分发数据Buffer

private:

    void Clear();

public:

    const VkBuffer GetTransformVAB()const{return transform_vab_buffer;}
    const VkBuffer GetMaterialInstanceVAB()const{return material_instance_vab_buffer;}

    void Bind(Material *)const;

public:

    InstanceAssignmentBuffer(VulkanDevice *dev,Material *);
    ~InstanceAssignmentBuffer(){Clear();}

    void WriteNode(const DrawNodeList &);

    void UpdateTransformData(const DrawNodeList &,const int first,const int last);
    void UpdateMaterialInstanceData(const DrawNode *);
};//struct InstanceAssignmentBuffer
VK_NAMESPACE_END
