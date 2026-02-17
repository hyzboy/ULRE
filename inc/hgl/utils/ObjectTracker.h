#pragma once

// ObjectTracker 简化版本 - header only实现
// 所有功能都在头文件中，避免链接问题

#include<hgl/core/ObjectType.h>
#include<cstdint>
#include<atomic>
#include<vector>
#include<mutex>
#include<chrono>
#include<fstream>
#include<cstring>
#include<source_location>

namespace hgl::utils
{
    /**
     * 源位置捕获结构
     * 记录代码中的位置信息，用于追踪对象分配的调用栈
     */
    struct SourceLocation
    {
        const char* file;       // 指向编译期字符串常量
        uint32_t line;
        uint32_t column;
        const char* function;   // 指向编译期字符串常量
        
        SourceLocation() = default;
        
        SourceLocation(const std::source_location& loc)
            : file(loc.file_name())
            , line(loc.line())
            , column(loc.column())
            , function(loc.function_name())
        {
        }
    };

    /**
     * 对象分配事件
     * 记录单个对象的生命周期和分配栈
     */
    struct AllocationEvent
    {
        uint64_t object_id;                         // 唯一对象ID
        uint64_t timestamp;                         // 纳秒级时间戳
        hgl::core::ObjectTypeTag object_type;       // 对象类型
        char object_name[32];                       // 对象名称
        uint32_t stack_depth;                       // 栈深度
        SourceLocation stack[64];                   // 调用栈（最多64层）
        
        AllocationEvent()
            : object_id(0), timestamp(0), object_type(hgl::core::ObjectTypeTag::None),
              stack_depth(0)
        {
            std::memset(object_name, 0, sizeof(object_name));
        }
    };

    /**
     * 对象ID生成器
     * 原子自增，无锁，线程安全
     */
    class ObjectIdGenerator
    {
    private:
        std::atomic<uint64_t> next_id{1};
        
    public:
        ObjectIdGenerator() = default;
        ~ObjectIdGenerator() = default;
        
        uint64_t allocate() noexcept
        {
            return next_id.fetch_add(1, std::memory_order_relaxed);
        }
    };

    /**
     * 栈捕获RAII助手
     * 用于自动push/pop thread_local栈
     */
    class ScopeCapture
    {
    public:
        ScopeCapture(const std::source_location& loc = std::source_location::current()) noexcept
        {
            if (g_object_tracker)
            {
                AllocationTracker<1000000>::allocation_stack.push_back(SourceLocation(loc));
            }
        }
        
        ~ScopeCapture() noexcept
        {
            if (g_object_tracker && !AllocationTracker<1000000>::allocation_stack.empty())
            {
                AllocationTracker<1000000>::allocation_stack.pop_back();
            }
        }
        
        // 禁用拷贝
        ScopeCapture(const ScopeCapture&) = delete;
        ScopeCapture& operator=(const ScopeCapture&) = delete;
    };

    /**
     * 本地内存分配追踪器
     * 使用环形缓冲区存储分配事件
     * 线程安全，支持多线程并发分配
     */
    template<size_t CAPACITY = 1000000>
    class AllocationTracker
    {
    private:
        std::vector<AllocationEvent> buffer;
        std::atomic<size_t> write_pos{0};
        std::mutex lock;
        
        // 使用 inline static 在头文件中初始化
        inline static ObjectIdGenerator id_generator;
        inline static thread_local std::vector<SourceLocation> allocation_stack;
        
    public:
        AllocationTracker()
        {
            buffer.resize(CAPACITY);
        }
        
        ~AllocationTracker() = default;
        
        /**
         * 分配新的对象ID，并记录当前分配栈
         */
        uint64_t record_allocation(
            const char* object_name,
            hgl::core::ObjectTypeTag object_type
        ) noexcept
        {
            uint64_t object_id = id_generator.allocate();
            
            AllocationEvent evt;
            evt.object_id = object_id;
            evt.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            evt.object_type = object_type;
            evt.stack_depth = (uint32_t)allocation_stack.size();
            
            // 拷贝名称
            if (object_name)
            {
                std::strncpy(evt.object_name, object_name, sizeof(evt.object_name) - 1);
                evt.object_name[sizeof(evt.object_name) - 1] = '\0';
            }
            
            // 拷贝栈
            size_t copy_depth = std::min(allocation_stack.size(), size_t(64));
            std::memcpy(evt.stack, allocation_stack.data(), 
                       copy_depth * sizeof(SourceLocation));
            evt.stack_depth = copy_depth;
            
            // 写入环形缓冲区
            {
                std::lock_guard<std::mutex> g(lock);
                size_t pos = write_pos.fetch_add(1, std::memory_order_relaxed) % CAPACITY;
                buffer[pos] = evt;
            }
            
            return object_id;
        }
        
        /**
         * 崩溃时将整个追踪缓冲区导出到文件
         * 二进制格式，易于离线分析
         */
        void dump_to_file(const std::string& filename) noexcept
        {
            try
            {
                std::ofstream f(filename, std::ios::binary);
                if (!f.is_open())
                {
                    std::fputs("[ObjectTracker] Failed to open file for dump\n", stderr);
                    return;
                }
                
                {
                    std::lock_guard<std::mutex> g(lock);
                    for (const auto& evt : buffer)
                    {
                        if (evt.object_id == 0)
                            continue;
                        
                        // 写入事件头
                        f.write((const char*)&evt.object_id, sizeof(evt.object_id));
                        f.write((const char*)&evt.timestamp, sizeof(evt.timestamp));
                        f.write((const char*)&evt.object_type, sizeof(evt.object_type));
                        f.write(evt.object_name, sizeof(evt.object_name));
                        f.write((const char*)&evt.stack_depth, sizeof(evt.stack_depth));
                        
                        // 写入栈帧
                        for (uint32_t i = 0; i < evt.stack_depth; i++)
                        {
                            // 存储指针哈希值而不是指针本身
                            uint64_t file_hash = (uint64_t)(uintptr_t)evt.stack[i].file;
                            uint64_t func_hash = (uint64_t)(uintptr_t)evt.stack[i].function;
                            
                            f.write((const char*)&file_hash, sizeof(file_hash));
                            f.write((const char*)&evt.stack[i].line, sizeof(evt.stack[i].line));
                            f.write((const char*)&evt.stack[i].column, sizeof(evt.stack[i].column));
                            f.write((const char*)&func_hash, sizeof(func_hash));
                        }
                    }
                }
                
                std::fprintf(stderr, "[ObjectTracker] Dumped %zu bytes to %s\n", 
                           f.tellp(), filename.c_str());
            }
            catch (const std::exception& e)
            {
                std::fprintf(stderr, "[ObjectTracker] Exception during dump: %s\n", e.what());
            }
        }
        
        /**
         * 获取已记录的事件数量（有效事件）
         */
        size_t get_event_count() const noexcept
        {
            std::lock_guard<std::mutex> g(lock);
            size_t count = 0;
            for (const auto& evt : buffer)
            {
                if (evt.object_id != 0)
                    count++;
            }
            return count;
        }
        
    private:
        friend class ScopeCapture;
        
        static void push_location(const std::source_location& loc) noexcept
        {
            allocation_stack.push_back(SourceLocation(loc));
        }
        
        static void pop_location() noexcept
        {
            if (!allocation_stack.empty())
            {
                allocation_stack.pop_back();
            }
        }
    };

    // 全局实例（嵌入在头文件中以支持template）
    inline AllocationTracker<1000000>* g_object_tracker = nullptr;

    /**
     * 初始化全局对象追踪器
     */
    inline void initialize_object_tracker()
    {
        if (!g_object_tracker)
        {
            g_object_tracker = new AllocationTracker<1000000>();
        }
    }

    /**
     * 清理全局对象追踪器
     */
    inline void shutdown_object_tracker()
    {
        if (g_object_tracker)
        {
            delete g_object_tracker;
            g_object_tracker = nullptr;
        }
    }

} // namespace hgl::utils

// 便利宏：快速记录对象分配
#define HGL_TRACK_ALLOCATION(name, type) \
    (hgl::utils::g_object_tracker ? hgl::utils::g_object_tracker->record_allocation(name, type) : 0)

// 便利宏：生成栈捕获范围
#define HGL_CAPTURE_SCOPE() hgl::utils::ScopeCapture __scope__

