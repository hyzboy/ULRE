#ifndef HGL_GRAPH_GPU_DATA_ARRAY_INCLUDE
#define HGL_GRAPH_GPU_DATA_ARRAY_INCLUDE

#include<hgl/graph/VKBuffer.h>
namespace hgl
{
    namespace graph
    {
        /**
         * GPU数据阵列         
         **/
        class GPUDataArray
        {
            uint32_t alloc_count;
            uint32_t count;
            uint32_t item_size;

            GPUBuffer *     buf_gpu;
            uint8 *         buf_cpu;

            uint32_t *      offset;

        public:

            GPUDataArray(const uint32_t s=0)
            {
                alloc_count=0;
                count=0;
                item_size=s;
                buf_gpu=nullptr;
                buf_cpu=nullptr;
                offset=nullptr;
            }

            virtual ~GPUDataArray()
            {
                Clear();
            }

            virtual void Clear()
            {
                alloc_count=0;
                count=0;

                if(buf_gpu)
                {
                    buf_gpu->Unmap();
                    delete buf_gpu;
                    buf_gpu=nullptr;
                }

                SAFE_CLEAR_ARRAY(buf_cpu);
                SAFE_CLEAR_ARRAY(offset);
            }

            bool Init(const uint32_t c,const uint32_t s=0)
            {
                if(s
                 &&item_size<=0)
                {
                    if(s>HGL_SIZE_1KB*64)
                        return(false);

                    item_size=s;
                }

                count=c;

                if(count*item_size<=0)
                    return(false);
                
                buf_cpu=new 
            }
        };//class GPUDataArray
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_GPU_DATA_ARRAY_INCLUDE
