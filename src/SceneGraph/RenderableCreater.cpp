#include<hgl/graph/RenderableCreater.h>
#include<hgl/graph/vulkan/VKShaderModule.h>

namespace hgl
{
    namespace graph
    {
        RenderableCreater::RenderableCreater(SceneDB *sdb,vulkan::Material *m)
        {
            db  =sdb;
            mtl =m;
            vsm =mtl->GetVertexShaderModule();

            render_obj      =nullptr;
            vertices_number =0;
            vb_vertex       =nullptr;
            ibo             =nullptr;
        }

        RenderableCreater::~RenderableCreater()
        {
            SAFE_CLEAR(render_obj);
        }

        bool RenderableCreater::Init(const uint32 count)
        {
            if(count<=0)return(false);

            vertices_number=count;

            render_obj=mtl->CreateRenderable(vertices_number);

            return(true);
        }

        VertexBufferCreater *RenderableCreater::Bind(const AnsiString &name)
        {
            if(!vsm)return(false);

            VertexBufferCreater *vb;

            if(vb_map.Get(name,vb))
                return vb;

            const vulkan::ShaderStage *ss=vsm->GetStageInput(name);

            if(!ss)
                return(nullptr);

            vb=CreateVB(ss->base_type,ss->component,vertices_number);

            vb_map.Add(name,vb);

            return vb;
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

            if(vb_map.GetCount()!=si_count)
                return(nullptr);


        }
    }//namespace graph
}//namespace hgl
