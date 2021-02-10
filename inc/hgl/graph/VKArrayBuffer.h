#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>
namespace hgl
{
    namespace graph
    {
        /**
        * GPU�������л�����<br>
        * �����ڴ�������ͬ��ʽ�����ݣ������ڶ������Ⱦ��instance��
        */
        class GPUArrayBuffer
        {
        protected:

            uint32_t    item_size;                          ///<�������ݳ���
            uint32_t    alloc_count;                        ///<�ܼƷ�������ݸ���
            uint32_t    count;                              ///<ʵ��ʹ�õ����ݸ���

            GPUBuffer * buf_gpu;                            ///<ʵ������GPU������
            uint8 *     buf_cpu;
            uint32_t *  offset;                             ///<����ƫ�Ƶ�ַ

        public:

            /**
            * ���๹�캯��
            * @param s �������ݳ���
            * @param c ���ݸ���
            */
            GPUArrayBuffer(const uint32_t s=0,const uint32_t c=0);
            virtual ~GPUArrayBuffer();

            void Clear();                                   ///<��ջ�����
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
