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

            vertices_number =0;
            vabc_vertex     =nullptr;
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

            VertexAttribBufferCreater *vabc;

            if(vabc_maps.Get(name,vabc))
                return vabc;

            const vulkan::ShaderStage *ss=vsm->GetStageInput(name);

            if(!ss)
                return(nullptr);

            vabc=hgl::graph::CreateVABCreater(ss->base_type,ss->component,vertices_number);

            vabc_maps.Add(name,vabc);

            return vabc;
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
                
            }
        }
    }//namespace graph
}//namespace hgl
