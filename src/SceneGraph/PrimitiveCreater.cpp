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
            
            const auto *va=vil->GetConfig(name);

            if(!va)
                return(nullptr);

            ShaderStageBind *ssb;

            if(ssb_map.Get(name,ssb))
                return ssb->data;

            VAD *vad=hgl::graph::CreateVertexAttribData(vertices_number,va->format,va->vec_size,va->stride);

            if(!vad)
                return(nullptr);

            ssb=new ShaderStageBind;

            ssb->data   =vad;
            ssb->name   =name;
            ssb->binding=va->binding;
            
            ssb->vbo    =nullptr;

            ssb_map.Add(name,ssb);

            return ssb->data;
        }

        bool PrimitiveCreater::WriteVAD(const AnsiString &name,const void *data,const uint32_t bytes)
        {
            if(!vil)return(false);
            if(name.IsEmpty())return(false);
            if(!data)return(false);
            if(!bytes)return(false);
            
            ShaderStageBind *ssb;

            if(ssb_map.Get(name,ssb))
                return false;

            const auto *va=vil->GetConfig(name);

            if(!va)
                return(false);

            if(va->stride*vertices_number!=bytes)
                return(false);
               
            ssb=new ShaderStageBind;

            ssb->data   =nullptr;
            ssb->name   =name;
            ssb->binding=va->binding;

            ssb->vbo    =db->CreateVBO(va->format,vertices_number,data);

            ssb_map.Add(name,ssb);

            return true;
        }

        uint16 *PrimitiveCreater::CreateIBO16(uint count,const uint16 *data)
        {
            if(ibo)return(nullptr);

            ibo=db->CreateIBO16(count,data);
            return (uint16 *)ibo->Map();
        }

        uint32 *PrimitiveCreater::CreateIBO32(uint count,const uint32 *data)
        {
            if(ibo)return(nullptr);

            ibo=db->CreateIBO32(count,data);
            return (uint32 *)ibo->Map();
        }

        Primitive *PrimitiveCreater::Finish()
        {
            const uint si_count=vil->GetAttrCount();

            if(ssb_map.GetCount()!=si_count)
                return(nullptr);

            Primitive *primitive=db->CreatePrimitive(vertices_number);

            const auto *sp=ssb_map.GetDataList();
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
