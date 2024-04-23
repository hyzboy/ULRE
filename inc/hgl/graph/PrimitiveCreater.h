#ifndef HGL_GRAPH_PRIMITIVE_CREATER_INCLUDE
#define HGL_GRAPH_PRIMITIVE_CREATER_INCLUDE

#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKShaderModule.h>
namespace hgl
{
    namespace graph
    {
        class VertexDataManager;

        /**
         * 可绘制图元创建器
         */
        class PrimitiveCreater
        {
        protected:

            GPUDevice *device;
            const GPUPhysicalDevice *phy_device;

            VertexDataManager *vdm;
            RenderResource *db;

            const VIL *vil;

        protected:

            VkDeviceSize        vertices_number;
            VkDeviceSize        index_number;

            IndexBuffer *       ibo;
            void *              ibo_map;
            VABAccessMap        vbo_map;

        protected:

            bool AcquirePVB(VABAccess *,const AnsiString &,const void *data);                                     ///<请求一个顶点属性数据区

            void ClearAllData();

        public:

            PrimitiveCreater(RenderResource *sdb,const VIL *);
            PrimitiveCreater(VertexDataManager *);
            virtual ~PrimitiveCreater();

            virtual bool                    Init(const uint32 vertices_count,const uint32 index_count,IndexType it=IndexType::AUTO);                 ///<初始化，参数为顶点数量

            template<typename T> 
                    T *                     AccessVAB(const AnsiString &name)                                           ///<创建一个顶点属性数据缓冲区以及访问器
                    {
                        const VkFormat format=vil->GetVulkanFormat(name);

                        if(format!=T::GetVulkanFormat())
                            return(nullptr);

                        VABAccess vad;
                        if(!this->AcquirePVB(&vad,name,nullptr))
                            return(nullptr);

                        T *access=T::Create(vertices_number,vad.map_ptr);

                        access->Begin();

                        return access;
                    }

                    bool                    WriteVAB(const AnsiString &name,const void *data,const uint32_t bytes);     ///<直接写入顶点属性数据

                    const IndexType         GetIndexType()const{return ibo?ibo->GetType():IndexType::ERR;}              ///<取得索引数据类型

                    template<typename T> T *AccessIBO()
                    {
                        if(!ibo)return(nullptr);
                        if(ibo->GetStride()!=sizeof(T))return(nullptr);

                        return (T *)ibo_map;
                    }

                    template<typename T> bool WriteIBO(const T *data)
                    {
                        if(!ibo)return(false);
                        if(ibo->GetStride()!=sizeof(T))return(false);

                        hgl_cpy<T>((T *)ibo_map,data,index_number);

                        return(true);
                    }

                    //template<typename T,IndexType IT>
                    //T *                     CreateIBO(const uint count,const T *data=nullptr)                           ///<创建索引缓冲区
                    //{
                    //    if(ibo)
                    //        return(nullptr);
                    //
                    //    ibo=db->CreateIBO(IT,count,data);

                    //    if(!ibo)
                    //        return(nullptr);

                    //    return (T *)ibo->Map();
                    //}

                    //uint8 *                 CreateIBO8 (uint count,const uint8  *data=nullptr){return CreateIBO<uint8 ,IndexType::U8 >(count,data);}                         ///<创建8位的索引缓冲区
                    //uint16 *                CreateIBO16(uint count,const uint16 *data=nullptr){return CreateIBO<uint16,IndexType::U16>(count,data);}                         ///<创建16位的索引缓冲区
                    //uint32 *                CreateIBO32(uint count,const uint32 *data=nullptr){return CreateIBO<uint32,IndexType::U32>(count,data);}                         ///<创建32位的索引缓冲区

            virtual Primitive *             Finish(const AnsiString &);                                                                   ///<结束并创建可渲染对象
        };//class PrimitiveCreater
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_PRIMITIVE_CREATER_INCLUDE
