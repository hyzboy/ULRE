#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VertexDataManager.h>

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

        PrimitiveCreater::PrimitiveCreater(VertexDataManager *_vdm)
        {
            vdm=_vdm;
            vil=vdm->GetVIL();

            vertices_number =0;
            ibo             =nullptr;
        }

        bool PrimitiveCreater::Init(const uint32 count)
        {
            if(count<=0)return(false);

            vertices_number=count;

            return(true);
        }

        PrimitiveCreater::PrimitiveVertexBuffer *PrimitiveCreater::AcquirePVB(const AnsiString &name,const void *data)
        {
            if(!vil)return(nullptr);
            if(name.IsEmpty())return(nullptr);
            
            const VertexInputFormat *vif=vil->GetConfig(name);

            if(!vif)
                return(nullptr);

            PrimitiveVertexBuffer *pvb;

            if(vbo_map.Get(name,pvb))
                return pvb;

            pvb=new PrimitiveVertexBuffer;

            pvb->vbo    =db->CreateVBO(vif->format,vertices_number,data);

            if(!data)
                pvb->map_data=pvb->vbo->Map();
            else
                pvb->map_data=nullptr;

            vbo_map.Add(name,pvb);

            return pvb;
        }

        bool PrimitiveCreater::WriteVBO(const AnsiString &name,const void *data,const uint32_t bytes)
        {
            if(!vil)return(false);
            if(name.IsEmpty())return(false);
            if(!data)return(false);
            if(!bytes)return(false);
            
            const VertexInputFormat *vif=vil->GetConfig(name);

            if(!vif)
                return(false);

            if(vif->stride*vertices_number!=bytes)
                return(false);

            return AcquirePVB(name,data);
        }

        void PrimitiveCreater::ClearAllData()
        {
            if(vbo_map.GetCount()>0)
            {
                const auto *sp=vbo_map.GetDataList();
                for(uint i=0;i<vbo_map.GetCount();i++)
                {
                    if((*sp)->value->vbo)
                    {
                        (*sp)->value->vbo->Unmap();
                        delete (*sp)->value->vbo;
                    }
                
                    ++sp;
                }
            }

            if(ibo)
            {
                ibo->Unmap();
                delete ibo;
                ibo=nullptr;
            }
        }

        Primitive *PrimitiveCreater::Finish(const AnsiString &prim_name)
        {
            const uint si_count=vil->GetCount(VertexInputGroup::Basic);

            if(vbo_map.GetCount()!=si_count)
                return(nullptr);

            Primitive *primitive=db->CreatePrimitive(prim_name,vertices_number);

            if(ibo)
            {
                ibo->Unmap();
                primitive->Set(ibo);
            }

            const auto *sp=vbo_map.GetDataList();
            for(uint i=0;i<si_count;i++)
            {
                if((*sp)->value->vbo)
                {
                    if((*sp)->value->map_data)
                        (*sp)->value->vbo->Unmap();

                    primitive->Set((*sp)->key,(*sp)->value->vbo);
                }
                else
                {
                    //ClearAllData();
                    return(nullptr);
                }

                ++sp;
            }

            db->Add(primitive);

            return primitive;
        }
    }//namespace graph
}//namespace hgl
