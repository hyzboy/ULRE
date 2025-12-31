#pragma once
#include<hgl/graph/DrawNode.h>

VK_NAMESPACE_BEGIN

class MaterialInstance;

/*
* 渲染节点MaterialInstance数据管理
*/
class MaterialInstanceAssignmentBuffer
{
private:

    VulkanDevice *device;

    Material *material;

private:    //材质实例数据
    
    MaterialInstanceSets mi_set;

    uint32_t        material_instance_data_bytes;           ///<单个材质实例数据字节数
    DeviceBuffer *  material_instance_buffer;               ///<材质实例数据(UBO/SSBO)

    void StatMaterialInstance(const DrawNodeList &);
    
private:    //分发数据

    uint32 node_count;                  ///<节点数量

    VAB *material_instance_vab;         ///<材质实例ID分发数据VAB(R16UI格式)
    VkBuffer material_instance_vab_buffer;  ///<材质实例ID分发数据Buffer

private:

    void Clear();

public:

    const VkBuffer GetMaterialInstanceVAB()const{return material_instance_vab_buffer;}

    void BindMaterialInstance(Material *)const;

public:

    MaterialInstanceAssignmentBuffer(VulkanDevice *dev,Material *);
    ~MaterialInstanceAssignmentBuffer(){Clear();}

    void WriteNode(const DrawNodeList &);

    void UpdateMaterialInstanceData(const DrawNode *);
};//class MaterialInstanceAssignmentBuffer
VK_NAMESPACE_END
