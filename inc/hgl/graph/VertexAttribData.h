#ifndef HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE
#define HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE

#include<hgl/graph/VK.h>
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
            VAN_DEFINE(Position)
            VAN_DEFINE(Normal)
            VAN_DEFINE(BaseColor)
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

        /**
         * 顶点属性数据
         */
        class VertexAttribData                                                                      ///顶点属性数据
        {
            void *mem_data;                                                                         ///<内存中的数据

        protected:

            const uint32_t vec_size;                                                                ///<每个数据成员数(比如二维坐标为2、三维坐标为3)
                  uint32_t count;                                                                   ///<数据个数

            const uint32_t stride;                                                                  ///<每组数据字节数
            const uint32_t total_bytes;                                                             ///<字节数

                  VkFormat vk_format;                                                               ///<在Vulkan中的数据类型

        public:

            VertexAttribData(uint32_t c,uint32_t dc,uint32_t cs,VkFormat fmt):count(c),vec_size(dc),stride(cs),total_bytes(cs*c),vk_format(fmt)
            {
                mem_data = hgl_malloc(total_bytes);            //在很多情况下，hgl_malloc分配的内存是对齐的，这样有效率上的提升
            }

            virtual ~VertexAttribData()
            {
                if(mem_data)
                    hgl_free(mem_data);
            }

            const   VkFormat    GetVulkanFormat ()const{return vk_format;}                          ///<取得数据类型
            const   uint32_t    GetVecSize      ()const{return vec_size;}                           ///<取数缓冲区元数据成份数量
            const   uint32_t    GetCount        ()const{return count;}                              ///<取得数据数量
            const   uint32_t    GetStride       ()const{return stride;}                             ///<取得每一组数据字节数
                    void *      GetData         ()const{return mem_data;}                           ///<取得数据指针
            const   uint32_t    GetTotalBytes   ()const{return total_bytes;}                        ///<取得数据字节数
        };//class VertexAttribData

        using VAD=VertexAttribData;

        /**
         * 根据格式要求，创建对应的顶点属性数据区(VAD)
         * @param vertex_count  顶点数量
         * @param fmt           Vulkan格式
         * @param vec_size      vec数量
         * @param stride        单个数据字节数
         */
        VAD *CreateVertexAttribData(const uint32_t vertex_count,const VkFormat fmt,const int vec_size,const uint stride);
        //这个函数比较重要，就不搞成CreateVAD的简写了
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE
