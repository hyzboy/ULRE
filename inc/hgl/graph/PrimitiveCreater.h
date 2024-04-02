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
                int             binding;
                VBO *           vbo;
                void *          map_data;
            };//struct PrimitiveVertexBuffer

            using PVBMap=ObjectMap<AnsiString,PrimitiveVertexBuffer>;

        protected:

            RenderResource *db;

            const VIL *vil;

        protected:

            uint32              vertices_number;

            IndexBuffer *       ibo;
            PVBMap              vbo_map;

        protected:

            PrimitiveVertexBuffer *AcquirePVB(const AnsiString &,const void *data);                                     ///<请求一个顶点属性数据区

            void ClearAllData();

        public:

            PrimitiveCreater(RenderResource *sdb,const VIL *);
            virtual ~PrimitiveCreater()=default;

            virtual bool                    Init(const uint32 vertices_count);                                          ///<初始化，参数为顶点数量

            template<typename T> 
                    T *                     AccessVAD(const AnsiString &name)                                           ///<创建一个顶点属性缓冲区以及访问器
                    {
                        const VkFormat format=vil->GetVulkanFormat(name);

                        if(format!=T::GetVulkanFormat())
                            return(nullptr);

                        PrimitiveVertexBuffer *pvb=this->AcquirePVB(name,nullptr);

                        if(!pvb)
                            return(nullptr);

                        T *access=T::Create(vertices_number,pvb->map_data);

                        access->Begin();

                        return access;
                    }

                    bool                    WriteVAD(const AnsiString &name,const void *data,const uint32_t bytes);     ///<直接写入顶点属性数据

                    template<typename T,IndexType IT>
                    T *                     CreateIBO(const uint count,const T *data=nullptr)                            ///<创建索引缓冲区
                    {
                        if(ibo)
                            return(nullptr);
                    
                        ibo=db->CreateIBO(IT,count,data);

                        if(!ibo)
                            return(nullptr);

                        return (T *)ibo->Map();
                    }

                    uint8 *                 CreateIBO8 (uint count,const uint8  *data=nullptr){return CreateIBO<uint8 ,IndexType::U8 >(count,data);}                         ///<创建8位的索引缓冲区
                    uint16 *                CreateIBO16(uint count,const uint16 *data=nullptr){return CreateIBO<uint16,IndexType::U16>(count,data);}                         ///<创建16位的索引缓冲区
                    uint32 *                CreateIBO32(uint count,const uint32 *data=nullptr){return CreateIBO<uint32,IndexType::U32>(count,data);}                         ///<创建32位的索引缓冲区

            virtual Primitive *             Finish(const AnsiString &);                                                                   ///<结束并创建可渲染对象
        };//class PrimitiveCreater
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_PRIMITIVE_CREATER_INCLUDE
