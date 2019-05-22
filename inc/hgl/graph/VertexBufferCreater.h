#ifndef HGL_GRAPH_VERTEX_BUFFER_CREATER_INCLUDE
#define HGL_GRAPH_VERTEX_BUFFER_CREATER_INCLUDE

#include<hgl/graph/vulkan/VK.h>
namespace hgl
{
    namespace graph
    {
        class VertexBufferCreater
        {
            void *mem_data;                                                                         ///<内存中的数据

        protected:

            const uint32_t dc_num;                                                                  ///<每个数据成员数(比如二维坐标为2、三维坐标为3)
            const uint32_t comp_stride;                                                             ///<单个成员数据字节数
                  uint32_t count;                                                                   ///<数据个数

            const uint32_t stride;                                                                  ///<每组数据字节数
            const uint32_t total_bytes;                                                             ///<字节数

            void *mem_end;                                                                          ///<内存数据区访问结束地址

        public:

            VertexBufferCreater(uint32_t c,uint32_t dc,uint32_t cs):count(c),dc_num(dc),comp_stride(cs),stride(dc*cs),total_bytes(dc*cs*c)
            {
                mem_data = hgl_malloc(total_bytes);            //在很多情况下，hgl_malloc分配的内存是对齐的，这样有效率上的提升
                mem_end = ((char *)mem_data) + total_bytes;
            }

            virtual ~VertexBufferCreater()
            {
                if(mem_data)
                    hgl_free(mem_data);
            }

            virtual VkFormat    GetDataType     ()const=0;                                          ///<取得数据类型
//            const   uint32_t    GetDataBytes    ()const{return comp_stride;}                        ///<取得每数据字节数
//            const   uint32_t    GetComponent    ()const{return dc_num;  }                           ///<取数缓冲区元数据数量
            const   uint32_t    GetCount        ()const{return count;   }                           ///<取得数据数量
            const   uint32_t    GetStride       ()const{return stride;}                             ///<取得每一组数据字节数
                    void *      GetData         ()const{return mem_data;}                           ///<取得数据指针
            const   uint32_t    GetTotalBytes   ()const{return total_bytes;   }                     ///<取得数据字节数
        };//class VertexBufferCreater
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_BUFFER_CREATER_INCLUDE
