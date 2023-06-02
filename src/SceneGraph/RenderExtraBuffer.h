#pragma once
#include<hgl/graph/VKVertexAttribBuffer.h>

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

/*
* 渲染节点额外提供的数据
*/
struct RenderExtraBuffer
{
    uint node_count;                            ///<渲染节点数量
    uint mi_count;                              ///<材质实例数量

    uint mi_size;                               ///<单个材质实例数量长度
    DeviceBuffer *mi_data_buffer;               ///<材质实例数据UBO/SSBO

    VBO *mi_id;
    VkBuffer mi_id_buffer;

//    VBO *bone_id,*bone_weight;
//    VkBuffer bone_id_buffer,bone_weight_buffer;

    VBO *l2w_vbo[4];
    VkBuffer l2w_buffer[4];

public:

    RenderExtraBuffer()
    {
        hgl_zero(*this);
    }

    ~RenderExtraBuffer()
    {
        Clear();
    }

    void ClearNode()
    {
        SAFE_CLEAR(l2w_vbo[0])
        SAFE_CLEAR(l2w_vbo[1])
        SAFE_CLEAR(l2w_vbo[2])
        SAFE_CLEAR(l2w_vbo[3])
        node_count=0;
    }

    void ClearMI()
    {
        SAFE_CLEAR(mi_id)
        SAFE_CLEAR(mi_data_buffer);
        mi_count=0;
        mi_size=0;
    }

    void Clear()
    {
        ClearNode();
        ClearMI();

//        SAFE_CLEAR(bone_id)
//        SAFE_CLEAR(bone_weight)
    }

    void NodeAlloc(GPUDevice *dev,const uint c)
    {
        ClearNode();
        node_count=power_to_2(c);

        for(uint i=0;i<4;i++)
        {
            l2w_vbo[i]=dev->CreateVBO(VF_V4F,node_count);
            l2w_buffer[i]=l2w_vbo[i]->GetBuffer();
        }
    }

    void MIAlloc(GPUDevice *dev,const uint c,const uint mis)
    {
        ClearMI();
        if(c<=0||mi_size<=0)return;
    
        mi_count=power_to_2(c);
        mi_size=mis;

        mi_id=dev->CreateVBO(VF_V1U8,mi_count);
        mi_id_buffer=mi_id->GetBuffer();

        mi_data_buffer=dev->CreateUBO(mi_count*mi_size);
    }

    void WriteLocalToWorld(RenderNode *render_node,const uint count)
    {
        RenderNode *rn;
        glm::vec4 *tp;

        for(uint col=0;col<4;col++)
        {
            tp=(glm::vec4 *)(l2w_vbo[col]->Map());

            rn=render_node;

            for(uint i=0;i<count;i++)
            {
                *tp=rn->local_to_world[col];
                ++tp;
                ++rn;
            }

            l2w_vbo[col]->Unmap();
        }
    }

    void WriteMaterialInstance(RenderNode *render_node,const uint count,const MaterialInstanceSets &mi_set)
    {
        //MaterialInstance ID
        {
            uint8 *tp=(uint8 *)(mi_id->Map());

            for(uint i=0;i<count;i++)
            {
                *tp=mi_set.Find(render_node->ri->GetMaterialInstance());
                ++tp;
                ++render_node;
            }
            mi_id->Unmap();
        }

        //MaterialInstance Data
        {
            //const uint count=mi_set.GetCount();

            //uint8 *tp=(uint8 *)(mi_data_buffer->Map());
            //const MaterialInstance **mi=mi_set.GetData();

            //for(uint i=0;i<count;i++)
            //{
            //    memcpy(tp,(*mi)->GetData(),mi_size);
    
            //    ++mi;
            //    tp+=mi_size;
            //}

            //mi_data_buffer->Unmap();
        }
    }
};//struct RenderExtraBuffer
VK_NAMESPACE_END
