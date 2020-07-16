#include<hgl/graph/RenderableCreater.h>
#include<hgl/graph/vulkan/VKShaderModule.h>

namespace hgl
{
    namespace graph
    {
        RenderableCreater::RenderableCreater(SceneDB *sdb,vulkan::Material *m)
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

        VertexAttribBufferCreater *RenderableCreater::CreateVAB(const AnsiString &name)
        {
            if(!vsm)return(false);

            ShaderStageBind *ssb;

            if(vabc_maps.Get(name,ssb))
                return ssb->vabc;

            const vulkan::ShaderStage *ss=vsm->GetStageInput(name);

            if(!ss)
                return(nullptr);

            ssb=new ShaderStageBind;

            ssb->vabc=hgl::graph::CreateVABCreater(ss->base_type,ss->component,vertices_number);

            ssb->name=name;
            ssb->binding=ss->binding;

            vabc_maps.Add(name,ssb);

            return ssb->vabc;
        }

        uint16 *RenderableCreater::CreateIBO16(uint count,const uint16 *data)
        {
            if(!ibo)return(nullptr);

            ibo=db->CreateIBO16(count,data);
            return (uint16 *)ibo->Map();
        }

        uint32 *RenderableCreater::CreateIBO32(uint count,const uint32 *data)
        {
            if(!ibo)return(nullptr);

            ibo=db->CreateIBO32(count,data);
            return (uint32 *)ibo->Map();
        }

        vulkan::Renderable *RenderableCreater::Finish()
        {
            const uint si_count=vsm->GetStageInputCount();

            if(vabc_maps.GetCount()!=si_count)
                return(nullptr);

            vulkan::Renderable *render_obj=mtl->CreateRenderable(vertices_number);

            const auto *sp=vabc_maps.GetDataList();
            for(uint i=0;i<si_count;i++)
            {
                render_obj->Set((*sp)->right->binding,db->CreateVAB((*sp)->right->vabc));

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
