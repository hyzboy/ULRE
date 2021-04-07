#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/type/Collection.h>
namespace hgl
{
    namespace graph
    {
        /**
        * GPU数据阵列缓冲区<br>
        * 它用于储存多份相同格式的数据，常用于多物件渲染，instance等
        */
        template<typename T> class GPUArrayBuffer
        {
        protected:

            GPUDevice * device;

            Collection<T> *coll;

        private:
        
            GPUArrayBuffer(GPUDevice *device,const uint32_t s,const uint32_t c)
            {
                
            }

            friend class GPUDevice;

        public:

            virtual ~GPUArrayBuffer();

            void Clear();                                   ///<清空缓冲区

            bool Init(const uint32_t);                      ///<初始化并分配空间
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
