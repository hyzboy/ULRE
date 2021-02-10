#include<hgl/graph/VKArrayBuffer.h>

namespace hgl
{
    namespace graph
    {
        /**
        * 本类构造函数
        * @param s 单个数据长度
        * @param c 数据个数
        */
        GPUArrayBuffer::GPUArrayBuffer(const uint32_t s,const uint32_t c)
        {
            item_size=s;
            count=c;
            alloc_count=power_to_2(c);

            buf_gpu=nullptr;
            buf_cpu=nullptr;
            offset=nullptr;
        }

        GPUArrayBuffer::~GPUArrayBuffer()
        {
            SAFE_CLEAR(buf_gpu);
        }

        void GPUArrayBuffer::Clear()
        {
            count=0;
        }


    }//namespace graph
}//namespace hgl
