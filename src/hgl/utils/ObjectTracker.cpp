#include<hgl/utils/ObjectTracker.h>

namespace hgl::utils
{
    // 静态成员初始化
    template<size_t CAPACITY>
    ObjectIdGenerator AllocationTracker<CAPACITY>::id_generator;
    
    template<size_t CAPACITY>
    thread_local std::vector<SourceLocation> AllocationTracker<CAPACITY>::allocation_stack;

    // 全局追踪器实例
    AllocationTracker<1000000>* g_object_tracker = nullptr;

    // ScopeCapture 实现
    void ScopeCapture::push_location(const std::source_location& loc) noexcept
    {
        if (g_object_tracker)
        {
            AllocationTracker<1000000>::allocation_stack.push_back(SourceLocation(loc));
        }
    }

    void ScopeCapture::pop_location() noexcept
    {
        if (g_object_tracker && !AllocationTracker<1000000>::allocation_stack.empty())
        {
            AllocationTracker<1000000>::allocation_stack.pop_back();
        }
    }

} // namespace hgl::utils
