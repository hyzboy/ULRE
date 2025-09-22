#pragma once
#include<hgl/graph/DrawNode.h>

VK_NAMESPACE_BEGIN

class MaterialInstance;

/*
* 渲染节点额外提供的数据
*/
class InstanceAssignmentBuffer
{
    struct AssignData
    {
        uint16 transform;
        uint16 material_instance;
    };

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

    VAB *assign_vab;                    ///<分发数据VAB(RG16UI格式，R存L2W ID，G存材质实例ID)
    VkBuffer assign_buffer;             ///<分发数据Buffer

private:

    void Clear();

public:

    const VkBuffer GetVAB()const{return assign_buffer;}

    void Bind(Material *)const;

public:

    InstanceAssignmentBuffer(VulkanDevice *dev,Material *);
    ~InstanceAssignmentBuffer(){Clear();}

    void WriteNode(const DrawNodeList &);

    void UpdateTransformData(const DrawNodeList &,const int first,const int last);
    void UpdateMaterialInstanceData(const DrawNode *);
};//struct InstanceAssignmentBuffer
VK_NAMESPACE_END
