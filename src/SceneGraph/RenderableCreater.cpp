#include<hgl/graph/RenderableCreater.h>
#include<hgl/graph/VKShaderModule.h>

namespace hgl
{
    namespace graph
    {
        RenderableCreater::RenderableCreater(vulkan::Database *sdb,vulkan::Material *m)
        {
            db              =sdb;
            mtl             =m;
            vsm             =mtl->GetVertexShaderModule();

            vertices_number =0;
            ibo             =nullptr;
        }

        bool RenderableCreater::Init(const uint32 count)
        {
            if(count<=0)return(false);

            vertices_number=count;

            return(true);
        }

        VAD *RenderableCreater::CreateVAD(const AnsiString &name,const vulkan::ShaderStage *ss)
        {
            if(!ss)return(nullptr);

            ShaderStageBind *ssb;

            if(vab_maps.Get(name,ssb))
                return ssb->data;

            ssb=new ShaderStageBind;

            ssb->data   =hgl::graph::CreateVertexAttribData(&(ss->type),vertices_number);
            ssb->name   =name;
            ssb->binding=ss->binding;
            
            ssb->vab    =nullptr;

            vab_maps.Add(name,ssb);

            return ssb->data;
        }

        VAD *RenderableCreater::CreateVAD(const AnsiString &name)
        {
            if(!vsm)return(nullptr);
            if(name.IsEmpty())return(nullptr);

            const vulkan::ShaderStage *ss=vsm->GetStageInput(name);

            if(!ss)
                return(nullptr);

            return this->CreateVAD(name,ss);
        }

        bool RenderableCreater::WriteVAD(const AnsiString &name,const void *data,const uint32_t bytes)
        {
            if(!vsm)return(false);
            if(name.IsEmpty())return(false);
            if(!data)return(false);
            if(!bytes)return(false);
            
            ShaderStageBind *ssb;

            if(vab_maps.Get(name,ssb))
                return false;

            const vulkan::ShaderStage *ss=vsm->GetStageInput(name);

            if(!ss)
                return(false);

            if(ss->type.GetStride()*vertices_number!=bytes)
                return(false);
               
            ssb=new ShaderStageBind;

            ssb->data   =nullptr;
            ssb->name   =name;
            ssb->binding=ss->binding;

            ssb->vab    =db->CreateVAB(ss->format,vertices_number,data);

            vab_maps.Add(name,ssb);

            return true;
        }

        uint16 *RenderableCreater::CreateIBO16(uint count,const uint16 *data)
        {
            if(ibo)return(nullptr);

            ibo=db->CreateIBO16(count,data);
            return (uint16 *)ibo->Map();
        }

        uint32 *RenderableCreater::CreateIBO32(uint count,const uint32 *data)
        {
            if(ibo)return(nullptr);

            ibo=db->CreateIBO32(count,data);
            return (uint32 *)ibo->Map();
        }

        vulkan::Renderable *RenderableCreater::Finish()
        {
            const uint si_count=vsm->GetStageInputCount();

            if(vab_maps.GetCount()!=si_count)
                return(nullptr);

            vulkan::Renderable *render_obj=db->CreateRenderable(vertices_number);

            const auto *sp=vab_maps.GetDataList();
            for(uint i=0;i<si_count;i++)
            {
                if((*sp)->right->vab)
                    render_obj->Set((*sp)->left,(*sp)->right->vab);                    
                else
                    render_obj->Set((*sp)->left,db->CreateVAB((*sp)->right->data));

                ++sp;
            }

            if(ibo)
            {
                ibo->Unmap();
                render_obj->Set(ibo);
            }

            db->Add(render_obj);

            return render_obj;
        }
    }//namespace graph
}//namespace hgl
