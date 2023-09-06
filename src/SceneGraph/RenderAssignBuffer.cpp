#include"RenderAssignBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>

VK_NAMESPACE_BEGIN

VkBuffer RenderAssignBuffer::GetAssignVBO()const
{
    return vbo_assigns->GetBuffer();
}

void RenderAssignBuffer::Bind(MaterialInstance *mi)const
{
    const VIL *vil=mi->GetVIL();

    const uint assign_binding_count=vil->GetCount(VertexInputGroup::Assign);

    if(assign_binding_count<=0)return;
    
    mi->BindUBO(DescriptorSetType::PerFrame,mtl::SBS_LocalToWorld.name,ubo_l2w);
//        mi->BindUBO(DescriptorSetType::PerFrame,"Assign",assign_buffer->ubo_mi);
}

void RenderAssignBuffer::ClearNode()
{
    SAFE_CLEAR(ubo_l2w);
    SAFE_CLEAR(ubo_mi);
    SAFE_CLEAR(vbo_assigns);

    node_count=0;
}

void RenderAssignBuffer::Clear()
{
    ClearNode();
//        ClearMI();

//        SAFE_CLEAR(bone_id)
//        SAFE_CLEAR(bone_weight)
}

void RenderAssignBuffer::NodeAlloc(GPUDevice *dev,const uint c)
{
    ClearNode();
    node_count=power_to_2(c);

    ubo_l2w=dev->CreateUBO(node_count*sizeof(Matrix4f));
    //ubo_mi=dev->CreateUBO(node_count*sizeof(uint8));
    vbo_assigns=dev->CreateVBO(ASSIGN_VBO_FMT,node_count);
}

//void MIAlloc(GPUDevice *dev,const uint c,const uint mis)
//{
//    ClearMI();
//    if(c<=0||mi_size<=0)return;
//
//    mi_count=power_to_2(c);
//    mi_size=mis;

//    mi_id=dev->CreateVBO(VF_V1U8,mi_count);
//    mi_id_buffer=mi_id->GetBuffer();

//    mi_data_buffer=dev->CreateUBO(mi_count*mi_size);
//}

void RenderAssignBuffer::WriteLocalToWorld(RenderNode *render_node,const uint count)
{
    RenderNode *rn;

    //new l2w array in ubo
    {
        Matrix4f *tp=(hgl::Matrix4f *)(ubo_l2w->Map());
        uint16 *idp=(uint16 *)(vbo_assigns->Map());

        rn=render_node;

        for(uint i=0;i<count;i++)
        {
            *tp=rn->local_to_world;
            ++tp;

            *idp=i;
            ++idp;

            ++rn;
        }

        vbo_assigns->Unmap();
        ubo_l2w->Unmap();
    }
}

//void WriteMaterialInstance(RenderNode *render_node,const uint count,const MaterialInstanceSets &mi_set)
//{
//    //MaterialInstance ID
//    {
//        uint8 *tp=(uint8 *)(mi_id->Map());

//        for(uint i=0;i<count;i++)
//        {
//            *tp=mi_set.Find(render_node->ri->GetMaterialInstance());
//            ++tp;
//            ++render_node;
//        }
//        mi_id->Unmap();
//    }

//    //MaterialInstance Data
//    {
//        //const uint count=mi_set.GetCount();

//        //uint8 *tp=(uint8 *)(mi_data_buffer->Map());
//        //const MaterialInstance **mi=mi_set.GetData();

//        //for(uint i=0;i<count;i++)
//        //{
//        //    memcpy(tp,(*mi)->GetData(),mi_size);
//
//        //    ++mi;
//        //    tp+=mi_size;
//        //}

//        //mi_data_buffer->Unmap();
//    }
//}

VK_NAMESPACE_END
