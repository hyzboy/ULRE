#include"RenderAssignBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>

VK_NAMESPACE_BEGIN
RenderAssignBuffer::RenderAssignBuffer(GPUDevice *dev,const bool has_l2w,const uint mi_bytes)
{
    hgl_zero(*this);

    device=dev;

    mi_data_bytes=mi_bytes;
}

void RenderAssignBuffer::Bind(Material *mtl)const
{
    if(!mtl)return;

    if(!mtl->HasMI())
        return;

    mtl->BindUBO(DescriptorSetType::PerMaterial,mtl::SBS_MaterialInstance.name,ubo_mi);
}

void RenderAssignBuffer::Clear()
{
    SAFE_CLEAR(ubo_mi);
    SAFE_CLEAR(vbo_mi);

    SAFE_CLEAR(l2w_vbo[0])
    SAFE_CLEAR(l2w_vbo[1])
    SAFE_CLEAR(l2w_vbo[2])
    SAFE_CLEAR(l2w_vbo[3])

    node_count=0;
    mi_count=0;
}

#ifdef _DEBUG
namespace
{
    constexpr const char *l2w_buffer_name[]=
    {
        "VBO:Buffer:LocalToWorld:0",
        "VBO:Buffer:LocalToWorld:1",
        "VBO:Buffer:LocalToWorld:2",
        "VBO:Buffer:LocalToWorld:3"
    };

    constexpr const char *l2w_memory_name[]=
    {
        "VBO:Memory:LocalToWorld:0",
        "VBO:Memory:LocalToWorld:1",
        "VBO:Memory:LocalToWorld:2",
        "VBO:Memory:LocalToWorld:3"
    };
}
#endif//_DEBUG

void RenderAssignBuffer::Alloc(const uint nc,const uint mc)
{
    Clear();

    {
        node_count=nc;

        for(uint i=0;i<4;i++)
        {
            l2w_vbo[i]=device->CreateVBO(VF_V4F,node_count);
            l2w_buffer[i]=l2w_vbo[i]->GetBuffer();
        }
    }

    if(mi_data_bytes>0&&mc>0)
    {
        mi_count=mc;

        ubo_mi=device->CreateUBO(mi_data_bytes*mi_count);
    }

    vbo_mi=device->CreateVBO(MI_VBO_FMT,node_count);
    mi_buffer=vbo_mi->GetBuffer();

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            if(l2w_buffer[0])
            {
                for(int i=0;i<4;i++)
                {
                    du->SetBuffer(l2w_buffer[i],l2w_buffer_name[i]);
                    du->SetDeviceMemory(l2w_vbo[i]->GetVkMemory(),l2w_memory_name[i]);
                }
            }

            if(ubo_mi)
            {
                du->SetBuffer(ubo_mi->GetBuffer(),"UBO:Buffer:MaterialInstance");
                du->SetDeviceMemory(ubo_mi->GetVkMemory(),"UBO:Memory:MaterialInstance");
            }

            if(vbo_mi)
            {
                du->SetBuffer(vbo_mi->GetBuffer(),"VBO:Buffer:MaterialInstanceID");
                du->SetDeviceMemory(vbo_mi->GetVkMemory(),"VBO:Memory:MaterialInstanceID");
            }
        }
    #endif//_DEBUG
}

void RenderAssignBuffer::WriteNode(RenderNode *render_node,const uint count,const MaterialInstanceSets &mi_set)
{
    RenderNode *rn;

    Alloc(count,mi_set.GetCount());

    if(l2w_buffer[0])
    {
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

    if(ubo_mi)
    {
        uint8 *mip=(uint8 *)(ubo_mi->Map());

        for(MaterialInstance *mi:mi_set)
        {
            memcpy(mip,mi->GetMIData(),mi_data_bytes);
            mip+=mi_data_bytes;
        }

        ubo_mi->Unmap();

        uint16 *idp=(uint16 *)(vbo_mi->Map());

        {
            rn=render_node;

            for(uint i=0;i<count;i++)
            {
                *idp=mi_set.Find(rn->ri->GetMaterialInstance());
                ++idp;

                ++rn;
            }
        }

        vbo_mi->Unmap();
    }
}
VK_NAMESPACE_END
