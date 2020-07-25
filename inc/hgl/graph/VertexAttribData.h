﻿#ifndef HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE
#define HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE

#include<hgl/graph/vulkan/VK.h>
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

            const uint32_t dc_num;                                                                  ///<每个数据成员数(比如二维坐标为2、三维坐标为3)
                  uint32_t count;                                                                   ///<数据个数

            const uint32_t stride;                                                                  ///<每组数据字节数
            const uint32_t total_bytes;                                                             ///<字节数

                  VkFormat vk_format;                                                               ///<在Vulkan中的数据类型

        public:

            VertexAttribData(uint32_t c,uint32_t dc,uint32_t cs,VkFormat fmt):count(c),dc_num(dc),stride(cs),total_bytes(cs*c),vk_format(fmt)
            {
                mem_data = hgl_malloc(total_bytes);            //在很多情况下，hgl_malloc分配的内存是对齐的，这样有效率上的提升
            }

            virtual ~VertexAttribData()
            {
                if(mem_data)
                    hgl_free(mem_data);
            }

            const   VkFormat    GetVulkanFormat ()const{return vk_format;}                          ///<取得数据类型
            const   uint32_t    GetComponent    ()const{return dc_num;}                             ///<取数缓冲区元数据数量
            const   uint32_t    GetCount        ()const{return count;}                              ///<取得数据数量
            const   uint32_t    GetStride       ()const{return stride;}                             ///<取得每一组数据字节数
                    void *      GetData         ()const{return mem_data;}                           ///<取得数据指针
            const   uint32_t    GetTotalBytes   ()const{return total_bytes;}                        ///<取得数据字节数
        };//class VertexAttribData

        using VAD=VertexAttribData;

        /**
         * 根据格式要求，创建对应的顶点属性数据区(VAD)
         * @param base_type 基础格式
         * @param vecsize vec数量
         * @param vertex_count 顶点数量
         */
        VertexAttribData *CreateVertexAttribData(const vulkan::BaseType base_type,const uint32_t vecsize,const uint32_t vertex_count);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ATTRIB_DATA_INCLUDE
