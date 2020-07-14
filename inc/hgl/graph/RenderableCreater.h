#ifndef HGL_GRAPH_RENDERABLE_CREATER_INCLUDE
#define HGL_GRAPH_RENDERABLE_CREATER_INCLUDE

#include<hgl/graph/SceneDB.h>
#include<hgl/graph/VertexAttribBuffer.h>
namespace hgl
{
    namespace graph
    {
        namespace VertexAttribName
        {
            #define VAN_DEFINE(name)    constexpr char name[]=#name;
            VAN_DEFINE(Vertex)
            VAN_DEFINE(Normal)
            VAN_DEFINE(Color)
            VAN_DEFINE(Tangent)
            VAN_DEFINE(Bitangent)
            VAN_DEFINE(TexCoord)
            #undef VAN_DEFINE
        }//namespace VertexAttribName

        #define VAN VertexAttribName

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

            VertexAttribBufferCreater *     vabc_vertex;
            vulkan::IndexBuffer *           ibo;

            MapObject<AnsiString,VertexAttribBufferCreater> vabc_maps;

        public:

            RenderableCreater(SceneDB *sdb,vulkan::Material *m);
            virtual ~RenderableCreater();

            virtual bool                    Init(const uint32 count);

            virtual VertexAttribBufferCreater *   CreateVAB(const AnsiString &name);

            #define PreDefineCreateVAB(name)   \
            virtual VertexAttribBufferCreater *   Create##name##Buffer(){return CreateVAB(VAN::name));}

            PreDefineCreateVAB(Vertex)
            PreDefineCreateVAB(Normal)
            PreDefineCreateVAB(Color)
            PreDefineCreateVAB(Tangent)
            PreDefineCreateVAB(Bitangent)
            PreDefineCreateVAB(TexCoord)

            #undef PreDefineCreateVAB

                    uint16 *                CreateIBO16(uint count,const uint16 *data=nullptr);
                    uint32 *                CreateIBO32(uint count,const uint32 *data=nullptr);

            virtual vulkan::Renderable *    Finish();
        };//class RenderableCreater
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDERABLE_CREATER_INCLUDE
