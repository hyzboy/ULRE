#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKShaderModule.h>

namespace hgl
{
    namespace graph
    {
        PrimitiveCreater::PrimitiveCreater(RenderResource *sdb,const VIL *v)
        {
            db              =sdb;
            vil             =v;

            vertices_number =0;
            ibo             =nullptr;
        }

        bool PrimitiveCreater::Init(const uint32 count)
        {
            if(count<=0)return(false);

            vertices_number=count;

            return(true);
        }

        VAD *PrimitiveCreater::CreateVAD(const AnsiString &name)
        {
            if(!vil)return(nullptr);
            if(name.IsEmpty())return(nullptr);
            
            const VertexInputFormat *vif=vil->GetConfig(name);

            if(!vif)
                return(nullptr);

            PrimitiveVertexBuffer *pvb;

            if(vbo_map.Get(name,pvb))
                return pvb->data;

            VAD *vad=hgl::graph::CreateVertexAttribData(vertices_number,vif);

            if(!vad)
                return(nullptr);

            pvb=new PrimitiveVertexBuffer;

            pvb->data   =vad;
            pvb->name   =name;
            pvb->binding=vif->binding;
            
            pvb->vbo    =nullptr;

            vbo_map.Add(name,pvb);

            return pvb->data;
        }

        bool PrimitiveCreater::WriteVAD(const AnsiString &name,const void *data,const uint32_t bytes)
        {
            if(!vil)return(false);
            if(name.IsEmpty())return(false);
            if(!data)return(false);
            if(!bytes)return(false);
            
            PrimitiveVertexBuffer *pvb;

            if(vbo_map.Get(name,pvb))
                return false;

            const VertexInputFormat *vif=vil->GetConfig(name);

            if(!vif)
                return(false);

            if(vif->stride*vertices_number!=bytes)
                return(false);
               
            pvb=new PrimitiveVertexBuffer;

            pvb->data   =nullptr;
            pvb->name   =name;
            pvb->binding=vif->binding;

            pvb->vbo    =db->CreateVBO(vif->format,vertices_number,data);

            vbo_map.Add(name,pvb);

            return true;
        }

        Primitive *PrimitiveCreater::Finish(const AnsiString &prim_name)
        {
            const uint si_count=vil->GetCount(VertexInputGroup::Basic);

            if(vbo_map.GetCount()!=si_count)
                return(nullptr);

            Primitive *primitive=db->CreatePrimitive(prim_name,vertices_number);

            const auto *sp=vbo_map.GetDataList();
            for(uint i=0;i<si_count;i++)
            {
                if((*sp)->value->vbo)
                    primitive->Set((*sp)->key,(*sp)->value->vbo);
                else
                    primitive->Set((*sp)->key,db->CreateVBO((*sp)->value->data));

                ++sp;
            }

            if(ibo)
            {
                ibo->Unmap();
                primitive->Set(ibo);
            }

            db->Add(primitive);

            return primitive;
        }
    }//namespace graph
}//namespace hgl
