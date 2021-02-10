#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>
namespace hgl
{
    namespace graph
    {
        /**
        * GPU数据阵列缓冲区<br>
        * 它用于储存多份相同格式的数据，常用于多物件渲染，instance等
        */
        class GPUArrayBuffer
        {
        protected:

            uint32_t    item_size;                          ///<单个数据长度
            uint32_t    alloc_count;                        ///<总计分配的数据个数
            uint32_t    count;                              ///<实际使用的数据个数

            GPUBuffer * buf_gpu;                            ///<实际数据GPU缓冲区
            uint8 *     buf_cpu;
            uint32_t *  offset;                             ///<数据偏移地址

        public:

            /**
            * 本类构造函数
            * @param s 单个数据长度
            * @param c 数据个数
            */
            GPUArrayBuffer(const uint32_t s=0,const uint32_t c=0);
            virtual ~GPUArrayBuffer();

            void Clear();                                   ///<清空缓冲区
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
