#pragma once
#include<hgl/graph/DrawNode.h>

VK_NAMESPACE_BEGIN
// ubo_range大致分为三档:
//
//  16k: Mali-T系列或更早、Mali-G71为16k
// 
//  64k: 大部分手机与PC均为64k
// 
// >64k: Intel 核显与 PowerVR 为128MB，AMD显卡为4GB，可视为随显存无上限。
// 
// 我们使用uint8类型在vertex input中保存MaterialInstance ID，表示范围0-255。
// 所以MaterialInstance结构容量按16k/64k分为两个档次，64字节和256字节

// 如果一定要使用超过16K/64K硬件限制的容量，有两种办法
// 一、分多次渲染，使用UBO Offset偏移UBO数据区。
// 二、使用SSBO，但这样会导致性能下降，所以不推荐使用。
// 三、使用纹理保存材质实例数据，但这样会导致性能下降，所以不推荐使用。

// 但我们不解决这个问题
// 我们天然要求将材质实例数据分为两个等级，同时要求一次渲染不能超过256种材质实例。
// 所以 UBO Range为16k时，实例数据不能超过64字节。UBO Range为64k时，实例数据不能超过256字节。


struct DrawNode;
class MaterialInstance;

/*
* 渲染节点额外提供的数据
*/
class InstanceAssignmentBuffer
{
    struct AssignData
    {
        uint16 l2w;
        uint16 mi;
    };

    uint MaxTransformCount;

private:

    VulkanDevice *device;

    Material *material;

private:    //LocalToWorld矩阵数据

    uint32 transform_buffer_max_count;        ///<LocalToWorld矩阵最大数量
    DeviceBuffer *transform_buffer;           ///<LocalToWorld矩阵数据(UBO/SSBO)

    void StatL2W(const DrawNodeList &);

private:    //材质实例数据
    
    MaterialInstanceSets mi_set;

    uint32_t mi_data_bytes;             ///<单个材质实例数据字节数
    DeviceBuffer *mi_buffer;            ///<材质实例数据(UBO/SSBO)

    void StatMI(const DrawNodeList &);
    
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

    //下一代，将MaterialInstanceSets使用提前化，这样不用每一次绘制都重新写入MI DATA，可以提升效率。
    //虽然这样就不自动化了，但我们要的就是不自动化。
    //必须在外部全部准备好MaterialInstanceSets，然后一次性写入。
    //渲染时找不到就直接用0号材质实例

    //同样的LocalToWorld矩阵也可以提前化处理，这样对于静态物体，就只需要写入一次LocalToWorld矩阵了。

    void WriteNode(const DrawNodeList &);

    void UpdateTransformData(const DrawNodePointerList &,const int first,const int last);
    void UpdateMaterialInstanceData(const DrawNode *);
};//struct InstanceAssignmentBuffer
VK_NAMESPACE_END
