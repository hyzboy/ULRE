#pragma once

/**
 * ObjectBase - 所有可追踪资源的基类
 * 
 * 特性：
 * - 魔数验证（检测有效对象）
 * - 自动ID生成
 * - 创建/销毁位置追踪
 * - 防止二次销毁
 * - 即使崩溃也能恢复对象信息
 */

#include<hgl/core/ObjectType.h>
#include<hgl/utils/ObjectTracker.h>
#include<cstdint>
#include<atomic>
#include<string>
#include<source_location>
#include<unordered_map>
#include<mutex>
#include<memory>

namespace hgl::utils
{
    // 重用 ObjectTracker 中定义的 SourceLocation 和 ObjectIdGenerator
    // （避免重定义冲突）
    
    // SourceLocation 已由 ObjectTracker.h 定义
    // ObjectIdGenerator 已由 ObjectTracker.h 定义

    /**
     * 全局对象基类 - 所有需要追踪的资源必须继承此类
     * 
     * 使用方式：
     *   class MyResource : public ObjectBase
     *   {
     *   public:
     *       MyResource(const std::source_location& loc = std::source_location::current())
     *           : ObjectBase(hgl::core::ObjectTypeTag::SomeType, "MyResource", loc)
     *       {
     *       }
     *       
     *       virtual ~MyResource() override
     *       {
     *           // ObjectBase 析构函数自动处理
     *       }
     *   };
     */
    class ObjectBase
    {
    public:
        // 魔数用于验证对象有效性
        static constexpr uint64_t MAGIC_NUMBER = 0xDEADBEEFCAFEBABEULL;

    protected:
        uint64_t magic_;                // 魔数验证
        uint64_t object_id_;            // 唯一对象ID
        hgl::core::ObjectTypeTag type_; // 对象类型
        std::string object_name_;       // 对象名称
        SourceLocation creation_loc_;   // 创建位置
        SourceLocation destruction_loc_; // 销毁位置
        std::atomic<bool> destroyed_{false}; // 销毁标记

        // 静态ID生成器
        inline static ObjectIdGenerator id_generator_;

    public:
        /**
         * 构造函数 - 强制传入创建位置信息
         */
        ObjectBase(
            hgl::core::ObjectTypeTag type,
            const std::string& name,
            const std::source_location& creation_loc = std::source_location::current()
        ) noexcept
            : magic_(MAGIC_NUMBER)
            , object_id_(id_generator_.allocate())
            , type_(type)
            , object_name_(name)
            , creation_loc_(creation_loc)
        {
            // 注册到全局对象注册表
            register_object();
        }

        virtual ~ObjectBase() noexcept
        {
            if (!destroyed_.exchange(true, std::memory_order_release))
            {
                // 注销对象
                unregister_object();
            }
        }

        // ===== 禁止复制和移动 =====
        ObjectBase(const ObjectBase&) = delete;
        ObjectBase& operator=(const ObjectBase&) = delete;
        ObjectBase(ObjectBase&&) = delete;
        ObjectBase& operator=(ObjectBase&&) = delete;

    public:
        // ===== 查询接口 =====

        /**
         * 获取对象ID
         */
        uint64_t get_object_id() const noexcept
        {
            return object_id_;
        }

        /**
         * 获取对象类型
         */
        hgl::core::ObjectTypeTag get_object_type() const noexcept
        {
            return type_;
        }

        /**
         * 获取对象名称
         */
        const std::string& get_object_name() const noexcept
        {
            return object_name_;
        }

        /**
         * 获取创建位置
         */
        const SourceLocation& get_creation_location() const noexcept
        {
            return creation_loc_;
        }

        /**
         * 获取销毁位置
         */
        const SourceLocation& get_destruction_location() const noexcept
        {
            return destruction_loc_;
        }

        /**
         * 检查对象是否已销毁
         */
        bool is_destroyed() const noexcept
        {
            return destroyed_.load(std::memory_order_acquire);
        }

        /**
         * 验证对象有效性（通过魔数）
         */
        bool is_valid() const noexcept
        {
            return magic_ == MAGIC_NUMBER && !is_destroyed();
        }

        /**
         * 记录销毁位置
         */
        void set_destruction_location(const std::source_location& loc) noexcept
        {
            destruction_loc_ = SourceLocation(loc);
        }

        /**
         * 获取对象信息字符串
         */
        std::string to_string() const noexcept
        {
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "Object{ID=0x%I64x, Type=0x%x, Name=%s, Created at %s:%u in %s(), Destroyed=%s}",
                     object_id_, (uint32_t)type_, object_name_.c_str(),
                     creation_loc_.file, creation_loc_.line, creation_loc_.function,
                     is_destroyed() ? "YES" : "NO");
            return std::string(buf);
        }

    private:
        /**
         * 注册对象到全局注册表
         */
        void register_object() noexcept;

        /**
         * 从全局注册表注销对象
         */
        void unregister_object() noexcept;
    };

    /**
     * 全局对象注册表 - 追踪所有活跃对象
     * 用于：
     * - 实时监控对象生命周期
     * - 检测泄漏
     * - 崩溃恢复
     */
    class ObjectRegistry
    {
    private:
        std::unordered_map<uint64_t, ObjectBase*> objects_;
        std::mutex lock_;

        // 单例
        inline static ObjectRegistry* instance_ = nullptr;

    public:
        /**
         * 获取单例实例
         */
        static ObjectRegistry& get_instance() noexcept
        {
            if (!instance_)
            {
                instance_ = new ObjectRegistry();
            }
            return *instance_;
        }

        /**
         * 注册对象
         */
        void register_object(ObjectBase* obj) noexcept
        {
            if (!obj) return;
            
            std::lock_guard<std::mutex> lock(lock_);
            objects_[obj->get_object_id()] = obj;
        }

        /**
         * 注销对象
         */
        void unregister_object(uint64_t object_id) noexcept
        {
            std::lock_guard<std::mutex> lock(lock_);
            objects_.erase(object_id);
        }

        /**
         * 查找对象
         */
        ObjectBase* find_object(uint64_t object_id)
        {
            std::lock_guard<std::mutex> lock(lock_);
            auto it = objects_.find(object_id);
            if (it != objects_.end())
                return it->second;
            return nullptr;
        }

        /**
         * 获取对象总数
         */
        size_t get_object_count()
        {
            std::lock_guard<std::mutex> lock(lock_);
            return objects_.size();
        }

        /**
         * 列出所有对象
         */
        void list_all_objects()
        {
            std::lock_guard<std::mutex> lock(lock_);
            printf("[ObjectRegistry] Total objects: %zu\n", objects_.size());
            for (const auto& [id, obj] : objects_)
            {
                if (obj && obj->is_valid())
                {
                    printf("  %s\n", obj->to_string().c_str());
                }
            }
        }

        /**
         * 检测泄漏 - 返回泄漏对象数量
         */
        size_t report_leaks()
        {
            std::lock_guard<std::mutex> lock(lock_);
            size_t leak_count = 0;
            printf("\n[ObjectRegistry] === Leak Report ===\n");
            for (const auto& [id, obj] : objects_)
            {
                if (obj && obj->is_valid())
                {
                    printf("  LEAK: %s\n", obj->to_string().c_str());
                    leak_count++;
                }
            }
            printf("[ObjectRegistry] Total leaks: %zu\n\n", leak_count);
            return leak_count;
        }

    private:
        ObjectRegistry() = default;
        ~ObjectRegistry() = default;

        ObjectRegistry(const ObjectRegistry&) = delete;
        ObjectRegistry& operator=(const ObjectRegistry&) = delete;
    };

    // ===== 内联实现 =====
    inline void ObjectBase::register_object() noexcept
    {
        ObjectRegistry::get_instance().register_object(this);
    }

    inline void ObjectBase::unregister_object() noexcept
    {
        ObjectRegistry::get_instance().unregister_object(object_id_);
    }

} // namespace hgl::utils

// ===== 强制派生宏 =====

/**
 * 宏：强制派生类实现虚析构函数并继承ObjectBase
 * 
 * 使用方式：
 *   class MyResource : public ObjectBase
 *   {
 *   public:
 *       MyResource() : ObjectBase(ObjectTypeTag::SomeType, "MyResource") {}
 *       virtual ~MyResource() override
 *       {
 *           HGL_OBJECT_DESTROY_LOCATION();
 *       }
 *   };
 */
#define HGL_OBJECT_DESTROY_LOCATION() \
    set_destruction_location(std::source_location::current())

/**
 * 快速检查对象是否有效
 */
#define HGL_CHECK_OBJECT_VALID(obj) \
    do { \
        if (!(obj)->is_valid()) \
        { \
            fprintf(stderr, "[ERROR] Invalid object access: %s\n", (obj)->to_string().c_str()); \
            abort(); \
        } \
    } while(0)

/**
 * 快速从ID查找对象
 */
#define HGL_FIND_OBJECT(id, type) \
    static_cast<type*>(hgl::utils::ObjectRegistry::get_instance().find_object(id))

/**
 * 列出所有对象
 */
#define HGL_LIST_ALL_OBJECTS() \
    hgl::utils::ObjectRegistry::get_instance().list_all_objects()

/**
 * 检测并报告泄漏
 */
#define HGL_REPORT_LEAKS() \
    hgl::utils::ObjectRegistry::get_instance().report_leaks()

/**
 * 获取对象总数
 */
#define HGL_GET_OBJECT_COUNT() \
    hgl::utils::ObjectRegistry::get_instance().get_object_count()
