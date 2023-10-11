#ifndef HGL_GRAPH_PRIMITIVE_CREATER_INCLUDE
#define HGL_GRAPH_PRIMITIVE_CREATER_INCLUDE

#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKShaderModule.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 可绘制图元创建器
         */
        class PrimitiveCreater
        {
            struct PrimitiveVertexBuffer
            {
                AnsiString      name;
                uint            binding;
                VAD *           data    =nullptr;
                VBO *           vbo     =nullptr;

            public:

                ~PrimitiveVertexBuffer()
                {
                    SAFE_CLEAR(data);
                }
            };//struct PrimitiveVertexBuffer

            using PVBMap=ObjectMap<AnsiString,PrimitiveVertexBuffer>;

        protected:

            RenderResource *db;
            Material *mtl;

            const VIL *vil;

        protected:

            uint32              vertices_number;

            IndexBuffer *       ibo;
            PVBMap              vbo_map;

        public:

            PrimitiveCreater(RenderResource *sdb,const VIL *);
            virtual ~PrimitiveCreater()=default;

            virtual bool                    Init(const uint32 vertices_count);                                          ///<初始化，参数为顶点数量

                    VAD *                   CreateVAD(const AnsiString &name);                                          ///<创建一个顶点属性缓冲区

            template<typename T> 
                    T *                     AccessVAD(const AnsiString &name)                                           ///<创建一个顶点属性缓冲区以及访问器
                    {
                        const VkFormat format=vil->GetVulkanFormat(name);

                        if(format!=T::GetVulkanFormat())
                            return(nullptr);

                        VAD *vad=this->CreateVAD(name);

                        if(!vad)
                            return(nullptr);

                        T *access=T::Create(vad);

                        access->Begin();

                        return access;
                    }

                    bool                    WriteVAD(const AnsiString &name,const void *data,const uint32_t bytes);     ///<直接写入顶点属性数据

                    uint16 *                CreateIBO16(uint count,const uint16 *data=nullptr);                         ///<创建16位的索引缓冲区
                    uint32 *                CreateIBO32(uint count,const uint32 *data=nullptr);                         ///<创建32位的索引缓冲区

            virtual Primitive *             Finish(const AnsiString &);                                                                   ///<结束并创建可渲染对象
        };//class PrimitiveCreater
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_PRIMITIVE_CREATER_INCLUDE
