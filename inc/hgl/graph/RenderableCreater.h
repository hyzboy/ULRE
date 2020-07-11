#ifndef HGL_GRAPH_RENDERABLE_CREATER_INCLUDE
#define HGL_GRAPH_RENDERABLE_CREATER_INCLUDE

#include<hgl/graph/SceneDB.h>
#include<hgl/graph/VertexBuffer.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 可渲染对象创建器
         */
        class RenderableCreater
        {
        protected:

            SceneDB *db;
            vulkan::Material *mtl;

            const vulkan::VertexShaderModule *vsm;

        protected:

            vulkan::Renderable *    render_obj;

            uint32                  vertices_number;

            VertexBufferCreater *   vb_vertex;
            vulkan::IndexBuffer *   ibo;

            Map<AnsiString,VertexBufferCreater *> vb_map;

        public:

            RenderableCreater(SceneDB *sdb,vulkan::Material *m);
            virtual ~RenderableCreater();

            virtual bool                    Init(const uint32 count);

            virtual VertexBufferCreater *   Bind(const AnsiString &name);

                    uint16 *                CreateIBO16(uint count,const uint16 *data=nullptr);
                    uint32 *                CreateIBO32(uint count,const uint32 *data=nullptr);

            virtual vulkan::Renderable *    Finish();
        };//class GeometryCreater
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDERABLE_CREATER_INCLUDE
