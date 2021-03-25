#include<hgl/graph/VKArrayBuffer.h>

namespace hgl
{
    namespace graph
    {
        /**
        * 本类构造函数
        * @param d 设备指针
        * @param s 单个数据长度
        * @param c 数据个数
        */
        GPUArrayBuffer::GPUArrayBuffer(GPUDevice *d,const uint32_t s,const uint32_t c)
        {
            device=d;

            item_size=s;
            count=c;
            alloc_count=power_to_2(c);

            buf_gpu=nullptr;
            buf_cpu=nullptr;
            offset=nullptr;
        }

        GPUArrayBuffer::~GPUArrayBuffer()
        {
            SAFE_CLEAR_ARRAY(offset);
            SAFE_CLEAR(buf_gpu);
        }

        void GPUArrayBuffer::Clear()
        {
            count=0;
        }

        bool GPUArrayBuffer::Init(const uint32_t c)
        {
            if(c<=0)return(false);

            if(!buf_gpu)
            {
                count=c;
                alloc_count=power_to_2(count);

                total_bytes=item_size*alloc_count;

                if(total_bytes<=0)return(false);

                buf_gpu=device->CreateUBO(total_bytes);
                buf_cpu=(uint8 *)(buf_gpu->Map());

                offset=new uint32_t[alloc_count];
            }
            else
            {
            }
        }
    }//namespace graph
}//namespace hgl
