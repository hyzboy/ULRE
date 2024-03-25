#pragma once
#include<hgl/graph/RenderNode.h>

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

// 但我们不解决这个问题
// 我们天然要求将材质实例数据分为两个等级，同时要求一次渲染不能超过256种材质实例。
// 所以 UBO Range为16k时，实例数据不能超过64字节。UBO Range为64k时，实例数据不能超过256字节。


struct RenderNode;
class MaterialInstance;

/*
* 渲染节点额外提供的数据
*/
class RenderAssignBuffer
{
private:

    GPUDevice *device;

    uint node_count;                    ///<渲染节点数量

    VBO *l2w_vbo[4];
    VkBuffer l2w_buffer[4];

    uint32_t mi_data_bytes;             ///<材质实例数据字节数
    uint32_t mi_count;                  ///<材质实例数量
    DeviceBuffer *ubo_mi;               ///<材质实例数据
    
    VBO *vbo_mi;                        ///<材质实例ID(R16UI格式)
    VkBuffer mi_buffer;

private:

    void Alloc(const uint nc,const uint mc);

    void Clear();

public:

    const VkBuffer *GetLocalToWorldVBO()const{return l2w_buffer;}
    const VkBuffer  GetMIVBO          ()const{return mi_buffer;}

    void Bind(Material *)const;

public:

    RenderAssignBuffer(GPUDevice *dev,const bool has_l2w,const uint32_t mi_bytes);
    ~RenderAssignBuffer(){Clear();}

    void WriteNode(RenderNode *render_node,const uint count,const MaterialInstanceSets &mi_set);

};//struct RenderAssignBuffer
VK_NAMESPACE_END
