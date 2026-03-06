#pragma once

#include<hgl/type/String.h>
#include<hgl/core/ObjectType.h>
#include<cstdint>
#include<cstring>
#include<typeinfo>
#include<cassert>

namespace hgl::graph{

// 对象名字缓冲区大小常量
constexpr size_t OBJECT_NAME_BUFFER_SIZE = 128;
constexpr size_t OBJECT_NAME_MAX_DEPTH = 16;

// ObjectTypeTag 定义已移到 hgl::core::ObjectTypeTag
// 为了向后兼容，这里引用它
using ObjectTypeTag = hgl::core::ObjectTypeTag;

/**
 * 对象名字构建器
 * 使用轻量级结构延迟计算完整名字，只在真正需要输出时才生成字符串
 * 支持记录完整的分配堆栈：对象名 + 类型标签 + 调用者类型链
 */
struct ObjectNameBuilder
{
    char base_name[OBJECT_NAME_BUFFER_SIZE];         // 基础名字（定长，避免动态分配）
    ObjectTypeTag tags[OBJECT_NAME_MAX_DEPTH];     // 类型标签数组
    size_t type_hashes[OBJECT_NAME_MAX_DEPTH];     // 调用者类型哈希码（用于堆栈追踪）
    uint8_t depth;              // 当前层级深度

    // 构造函数：从C字符串创建
    ObjectNameBuilder(const char* name = "")
        : tags{}, type_hashes{}, depth(0)
    {
        std::strncpy(base_name, name, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
    }

    // 构造函数：从AnsiString创建
    ObjectNameBuilder(const AnsiString& name)
        : ObjectNameBuilder(name.c_str())
    {
    }

    // 从另一个 ObjectNameBuilder 复制
    ObjectNameBuilder(const ObjectNameBuilder& other)
        : depth(other.depth)
    {
        std::strncpy(base_name, other.base_name, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
        std::memcpy(tags, other.tags, sizeof(tags));
        std::memcpy(type_hashes, other.type_hashes, sizeof(type_hashes));
    }

    // 追加一个类型标签（不修改当前对象，返回新对象）
    ObjectNameBuilder Append(ObjectTypeTag tag) const
    {
        ObjectNameBuilder result = *this;
        if (result.depth < OBJECT_NAME_MAX_DEPTH)
        {
            result.tags[result.depth] = tag;
            result.depth++;
        }
        return result;
    }

    // 追加调用者的类型信息（通过模板支持任意类型）
    template<typename T>
    ObjectNameBuilder AppendType() const
    {
        ObjectNameBuilder result = *this;
        if (result.depth < OBJECT_NAME_MAX_DEPTH)
        {
            result.type_hashes[result.depth] = typeid(T).hash_code();
            result.depth++;
        }
        return result;
    }

    // 便捷函数：对于成员函数，直接追加 this 的类型
    ObjectNameBuilder AppendCallerType(const std::type_info& ti) const
    {
        ObjectNameBuilder result = *this;
        if (result.depth < OBJECT_NAME_MAX_DEPTH)
        {
            result.type_hashes[result.depth] = ti.hash_code();
            result.depth++;
        }
        return result;
    }

    // 获取类型标签的字符串表示（已移到 hgl::core::GetTagString）
    static const char* GetTagString(ObjectTypeTag tag)
    {
        return hgl::core::GetTagString(tag);
    }

    // 生成完整的层级名字（只在需要输出时调用）
    AnsiString ToString() const
    {
        AnsiString result(base_name);

        for (uint8_t i = 0; i < depth; ++i)
        {
            result += " <- ";

            // 输出类型哈希码（供符号查询使用）
            if (type_hashes[i] != 0)
            {
                char hash_buf[32];
                snprintf(hash_buf, sizeof(hash_buf), "0x%llx", (unsigned long long)type_hashes[i]);
                result += hash_buf;
            }

            // 输出标签（如果有）
            if (tags[i] != ObjectTypeTag::None)
            {
                result += ":";
                result += GetTagString(tags[i]);
            }
        }

        return result;
    }

    // 类型转换操作符（方便使用）
    operator AnsiString() const
    {
        return ToString();
    }

    // 赋值操作符 - 直接返回新的 builder（便于链式操作）
    ObjectNameBuilder& operator=(const ObjectNameBuilder& other)
    {
        std::memcpy(this, &other, sizeof(ObjectNameBuilder));
        return *this;
    }
};

// ===== 便利宏 =====

// 便利宏：用于在成员函数中快速创建带有调用者类型信息的 ObjectNameBuilder
// 用法: CreateBuffer(..., VK_NAME_FROM("UBO"), ...);
#define VK_NAME_FROM(obj_name) \
    ObjectNameBuilder(obj_name).AppendCallerType(typeid(*this))

// 便利宏：用于在自由函数中快速创建带有类型信息的 ObjectNameBuilder
// 用法: CreateBuffer(..., VK_NAME_WITH_TYPE("Buffer", SomeClass), ...);
#define VK_NAME_WITH_TYPE(obj_name, caller_type) \
    ObjectNameBuilder(obj_name).AppendCallerType(typeid(caller_type))

}//namespace hgl::graph
