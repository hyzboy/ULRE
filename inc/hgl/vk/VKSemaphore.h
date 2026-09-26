#ifndef HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
#define HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE

#include<hgl/vk/VK.h>
namespace hgl::graph{

/// 信号量类型。二进制信号量（WSI 的 acquire/present）value 恒 0；
/// timeline 信号量用 value 表达时点，允许跨帧、多等待方、重复信号（A1/A7 的车道排序）。
enum class SemaphoreType : uint8
{
    Binary   = 0,
    Timeline = 1,
};

class Semaphore
{
    VkDevice device;
    VkSemaphore sem;

    SemaphoreType   type;
    uint64_t        value;      ///< timeline：最近一次已提交的信号值；binary：恒 0

private:

    friend class VulkanDevice;

    Semaphore(VkDevice d,VkSemaphore s,SemaphoreType t=SemaphoreType::Binary,const uint64_t v=0)
    {
        device=d;
        sem=s;
        type=t;
        value=v;
    }

public:

    ~Semaphore();

    operator VkSemaphore(){return sem;}

    operator const VkSemaphore *()const{return &sem;}

    SemaphoreType GetType()     const{return type;}
    bool          IsTimeline()  const{return type==SemaphoreType::Timeline;}

    /// 最近一次提交使用的信号值（二进制恒 0）
    uint64_t      GetValue()    const{return value;}
    void          SetValue(const uint64_t v){value=v;}

    /// 取下一个信号值并推进（提交前调用，随后用它作为 signal value）
    uint64_t      NextValue(){return ++value;}
};//class Semaphore
}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
