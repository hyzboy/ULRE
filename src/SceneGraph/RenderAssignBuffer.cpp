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

void RenderAssignBuffer::Clear()
{
    SAFE_CLEAR(ubo_l2w);
    SAFE_CLEAR(ubo_mi);
    SAFE_CLEAR(vbo_assigns);

    node_count=0;
}

void RenderAssignBuffer::Alloc(const uint nc,const uint mc)
{
    Clear();

    {
        node_count=power_to_2(nc);

        ubo_l2w=device->CreateUBO(node_count*sizeof(Matrix4f));
    }

    {
        mi_count=power_to_2(mc);

        ubo_mi=device->CreateUBO(mi_count*mi_data_bytes);
    }

    vbo_assigns=device->CreateVBO(ASSIGN_VBO_FMT,node_count);
}

void RenderAssignBuffer::WriteNode(RenderNode *render_node,const uint count,const MaterialInstanceSets &mi_set)
{
    RenderNode *rn;

    Alloc(count,mi_set.GetCount());

    {
        uint8 *mip=(uint8 *)(ubo_mi->Map());

        for(MaterialInstance *mi:mi_set)
        {
//            memcpy(mip,mi->GetData(),mi_data_bytes);
            mip+=mi_data_bytes;
        }

        ubo_mi->Unmap();
    }

    uint16 *idp=(uint16 *)(vbo_assigns->Map());

    {
        Matrix4f *tp=(hgl::Matrix4f *)(ubo_l2w->Map());

        rn=render_node;

        for(uint i=0;i<count;i++)
        {
            *tp=rn->local_to_world;
            ++tp;

            *idp=i;
            ++idp;

            //*idp=mi_set.Find(rn->ri->GetMaterialInstance());
            //++idp;

            ++rn;
        }

        ubo_l2w->Unmap();
    }

    vbo_assigns->Unmap();
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
