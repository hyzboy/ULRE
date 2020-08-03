#ifndef HGL_GRAPH_RENDERABLE_CREATER_INCLUDE
#define HGL_GRAPH_RENDERABLE_CREATER_INCLUDE

#include<hgl/graph/SceneDB.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 预定义一些顶点属性名称，可用可不用。但一般默认shader会使用这些名称
         */
        namespace VertexAttribName
        {
            #define VAN_DEFINE(name)    constexpr char name[]=#name;
            VAN_DEFINE(Vertex)
            VAN_DEFINE(Normal)
            VAN_DEFINE(Color)
            VAN_DEFINE(Tangent)
            VAN_DEFINE(Bitangent)
            VAN_DEFINE(TexCoord)
            VAN_DEFINE(Metallic)
            VAN_DEFINE(Specular)
            VAN_DEFINE(Roughness)
            VAN_DEFINE(Emission)
            #undef VAN_DEFINE
        }//namespace VertexAttribName

        #define VAN VertexAttribName

        struct ShaderStageBind
        {
            AnsiString      name;
            uint            binding;
            VAD *           data    =nullptr;
            vulkan::VAB *   vab     =nullptr;

        public:

            ~ShaderStageBind()
            {
                SAFE_CLEAR(data);
            }
        };//struct ShaderStageBind

        using VADMaps=MapObject<AnsiString,ShaderStageBind>;

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

            uint32                  vertices_number;

            vulkan::IndexBuffer *   ibo;
            VADMaps                 vab_maps;

        protected:

            virtual VAD *CreateVAD(const AnsiString &name,const vulkan::ShaderStage *ss);                               ///<创建一个顶点属性缓冲区

        public:

            RenderableCreater(SceneDB *sdb,vulkan::Material *m);
            virtual ~RenderableCreater()=default;

            virtual bool                    Init(const uint32 count);                                                   ///<初始化，参数为顶点数量

                    VAD *                   CreateVAD(const AnsiString &name);                                          ///<创建一个顶点属性缓冲区

            template<typename T> 
                    T *                     CreateVADA(const AnsiString &name)                                          ///<创建一个顶点属性缓冲区以及访问器
                    {
                        const vulkan::ShaderStage *ss=vsm->GetStageInput(name);

                        if(!ss)
                            return(nullptr);

                        if(ss->format!=T::GetVulkanFormat())
                            return(nullptr);

                        VAD *vad=this->CreateVAD(name);

                        if(!vad)
                            return(nullptr);

                        T *vada=T::Create(vad);

                        vada->Begin();

                        return vada;
                    }

                    bool                    WriteVAD(const AnsiString &name,const void *data,const uint32_t bytes);     ///<直接写入顶点属性数据

                    uint16 *                CreateIBO16(uint count,const uint16 *data=nullptr);                         ///<创建16位的索引缓冲区
                    uint32 *                CreateIBO32(uint count,const uint32 *data=nullptr);                         ///<创建32位的索引缓冲区

            virtual vulkan::Renderable *    Finish();                                                                   ///<结束并创建可渲染对象
        };//class RenderableCreater
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDERABLE_CREATER_INCLUDE
