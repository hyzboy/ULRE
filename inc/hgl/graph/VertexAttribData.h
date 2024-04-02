#ifndef HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE
#define HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE

#include<hgl/graph/VK.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 顶点属性数据
         */
        class VertexAttribData                                                                      ///顶点属性数据
        {
            void *mem_data;                                                                         ///<内存中的数据

        protected:

            VkFormat format;

            uint32_t count;                                                                         ///<数据个数
            uint32_t total_bytes;                                                                   ///<字节数

        public:

            VertexAttribData(uint32_t c,const VkFormat vf,const uint32_t t)
            {
                count=c;
                format=vf;
                total_bytes=t;

                mem_data = hgl_malloc(total_bytes);            //在很多情况下，hgl_malloc分配的内存是对齐的，这样有效率上的提升
            }

            virtual ~VertexAttribData()
            {
                if(mem_data)
                    hgl_free(mem_data);
            }

            const   VkFormat    GetFormat       ()const{return format;}                             ///<取得数据类型
            const   uint32_t    GetCount        ()const{return count;}                              ///<取得数据数量
                    void *      GetData         ()const{return mem_data;}                           ///<取得数据指针
            const   uint32_t    GetTotalBytes   ()const{return total_bytes;}                        ///<取得数据字节数
        };//class VertexAttribData

        using VAD=VertexAttribData;
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE
