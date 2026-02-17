#include<hgl/utils/ObjectTracker.h>

namespace hgl::utils
{
    // 静态成员初始化
    template<size_t CAPACITY>
    ObjectIdGenerator AllocationTracker<CAPACITY>::id_generator;
    
    template<size_t CAPACITY>
    thread_local std::vector<SourceLocation> AllocationTracker<CAPACITY>::allocation_stack;

} // namespace hgl::utils
