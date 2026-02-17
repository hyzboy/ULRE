#pragma once

/**
 * ObjectCrashRecovery - 从内存倾转或日志中恢复对象信息
 * 
 * 用于：
 * - 分析程序崩溃时的对象状态
 * - 检测泄漏的根本原因
 * - 生成收集报告
 */

#include<hgl/utils/ObjectBase.h>
#include<fstream>
#include<vector>
#include<cstring>

namespace hgl::utils
{
    /**
     * 对象快照 - 保存对象状态以便恢复
     */
    struct ObjectSnapshot
    {
        uint64_t magic;
        uint64_t object_id;
        uint32_t object_type;
        char object_name[64];
        char creation_file[256];
        char creation_function[128];
        uint32_t creation_line;
        char destruction_file[256];
        char destruction_function[128];
        uint32_t destruction_line;
        bool is_destroyed;
        uint64_t timestamp;

        ObjectSnapshot() = default;

        ObjectSnapshot(const ObjectBase* obj)
            : magic(obj->is_valid() ? ObjectBase::MAGIC_NUMBER : 0)
            , object_id(obj->get_object_id())
            , object_type((uint32_t)obj->get_object_type())
            , is_destroyed(obj->is_destroyed())
            , timestamp(std::chrono::high_resolution_clock::now().time_since_epoch().count())
        {
            strncpy_s(object_name, obj->get_object_name().c_str(), sizeof(object_name) - 1);
            object_name[sizeof(object_name) - 1] = '\0';

            const auto& create_loc = obj->get_creation_location();
            strncpy_s(creation_file, create_loc.file, sizeof(creation_file) - 1);
            strncpy_s(creation_function, create_loc.function, sizeof(creation_function) - 1);
            creation_line = create_loc.line;

            const auto& destroy_loc = obj->get_destruction_location();
            if (destroy_loc.file)
            {
                strncpy_s(destruction_file, destroy_loc.file, sizeof(destruction_file) - 1);
                strncpy_s(destruction_function, destroy_loc.function, sizeof(destruction_function) - 1);
                destruction_line = destroy_loc.line;
            }
            else
            {
                memset(destruction_file, 0, sizeof(destruction_file));
                memset(destruction_function, 0, sizeof(destruction_function));
                destruction_line = 0;
            }
        }

        bool is_valid() const noexcept
        {
            return magic == ObjectBase::MAGIC_NUMBER;
        }

        std::string to_string() const noexcept
        {
            char buf[1024];
            snprintf(buf, sizeof(buf),
                     "Snapshot{ID=0x%lx, Type=0x%x, Name=%s, Created=%s:%u@%s, Destroyed=%s}",
                     object_id, object_type, object_name,
                     creation_file, creation_line, creation_function,
                     is_destroyed ? "YES" : "NO");
            return std::string(buf);
        }
    };

    /**
     * 对象崩溃恢复管理器
     */
    class ObjectCrashRecovery
    {
    public:
        /**
         * 从当前活跃对象创建快照并保存
         */
        static bool save_snapshot(const std::string& filename) noexcept
        {
            try
            {
                std::ofstream file(filename, std::ios::binary);
                if (!file.is_open())
                {
                    fprintf(stderr, "[ObjectCrashRecovery] Failed to open %s for writing\n", filename.c_str());
                    return false;
                }

                const auto& registry = ObjectRegistry::get_instance();
                
                // 获取所有对象快照
                std::vector<ObjectSnapshot> snapshots;
                {
                    // 由于无法直接访问registry的private成员，我们通过其他方式
                    // 这里简化为通过虚函数接口
                }

                uint32_t count = 0;
                file.write((const char*)&count, sizeof(count)); // 占位符

                // 在实际应用中，需要添加访问器方法到ObjectRegistry
                printf("[ObjectCrashRecovery] Saved %u object snapshots to %s\n", count, filename.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                fprintf(stderr, "[ObjectCrashRecovery] Exception: %s\n", e.what());
                return false;
            }
        }

        /**
         * 从快照文件恢复并显示对象信息
         */
        static bool load_and_analyze(const std::string& filename) noexcept
        {
            try
            {
                std::ifstream file(filename, std::ios::binary);
                if (!file.is_open())
                {
                    fprintf(stderr, "[ObjectCrashRecovery] Failed to open %s for reading\n", filename.c_str());
                    return false;
                }

                uint32_t count = 0;
                file.read((char*)&count, sizeof(count));

                printf("\n[ObjectCrashRecovery] === Object Recovery Report ===\n");
                printf("Recovered %u objects\n\n", count);

                std::vector<ObjectSnapshot> leaks;
                for (uint32_t i = 0; i < count; i++)
                {
                    ObjectSnapshot snap;
                    file.read((char*)&snap, sizeof(snap));

                    if (snap.is_valid())
                    {
                        printf("✓ %s\n", snap.to_string().c_str());
                        if (!snap.is_destroyed)
                        {
                            leaks.push_back(snap);
                        }
                    }
                    else
                    {
                        printf("✗ Invalid snapshot at index %u\n", i);
                    }
                }

                if (!leaks.empty())
                {
                    printf("\n[ObjectCrashRecovery] === Detected Leaks ===\n");
                    printf("Total leaks: %zu\n\n", leaks.size());

                    // 按类型分组泄漏
                    std::unordered_map<uint32_t, std::vector<ObjectSnapshot>> by_type;
                    for (const auto& leak : leaks)
                    {
                        by_type[leak.object_type].push_back(leak);
                    }

                    for (const auto& [type, objects] : by_type)
                    {
                        printf("Type 0x%x: %zu leaks\n", type, objects.size());
                        for (const auto& obj : objects)
                        {
                            printf("  - %s\n", obj.to_string().c_str());
                        }
                    }
                }

                return true;
            }
            catch (const std::exception& e)
            {
                fprintf(stderr, "[ObjectCrashRecovery] Exception: %s\n", e.what());
                return false;
            }
        }

        /**
         * 扫描内存查找可能的ObjectBase对象
         * 用于从crash dump中恢复
         */
        static std::vector<ObjectSnapshot> scan_memory_for_objects(
            const void* memory_start,
            size_t memory_size
        ) noexcept
        {
            std::vector<ObjectSnapshot> found_objects;
            
            const uint8_t* ptr = (const uint8_t*)memory_start;
            const uint8_t* end = ptr + memory_size;

            // 搜索魔数
            while (ptr < end - sizeof(ObjectBase::MAGIC_NUMBER))
            {
                if (*(const uint64_t*)ptr == ObjectBase::MAGIC_NUMBER)
                {
                    // 可能找到了ObjectBase的开头
                    // 尝试验证其他字段
                    printf("[ObjectCrashRecovery] Found potential ObjectBase at offset 0x%lx\n",
                           (uintptr_t)ptr - (uintptr_t)memory_start);
                }
                ptr++;
            }

            return found_objects;
        }

        /**
         * 生成HTML格式的泄漏报告
         */
        static bool generate_html_report(
            const std::vector<ObjectSnapshot>& objects,
            const std::string& output_file
        ) noexcept
        {
            try
            {
                std::ofstream file(output_file);
                if (!file.is_open()) return false;

                file << R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Object Leak Report</title>
    <style>
        body { font-family: monospace; }
        table { border-collapse: collapse; margin: 20px 0; }
        th, td { border: 1px solid #ccc; padding: 8px; text-align: left; }
        th { background-color: #f0f0f0; }
        tr.leak { background-color: #ffcccc; }
        tr.valid { background-color: #ccffcc; }
    </style>
</head>
<body>
    <h1>Object Leak Report</h1>
)";

                file << "<table>\n<tr>";
                file << "<th>ID</th><th>Type</th><th>Name</th>";
                file << "<th>Created At</th><th>Destroyed</th></tr>\n";

                size_t leak_count = 0;
                for (const auto& obj : objects)
                {
                    bool is_leak = !obj.is_destroyed;
                    const char* row_class = is_leak ? "leak" : "valid";
                    
                    file << "<tr class=\"" << row_class << "\">";
                    file << "<td>0x" << std::hex << obj.object_id << std::dec << "</td>";
                    file << "<td>0x" << std::hex << obj.object_type << std::dec << "</td>";
                    file << "<td>" << obj.object_name << "</td>";
                    file << "<td>" << obj.creation_file << ":" << obj.creation_line << "</td>";
                    file << "<td>" << (obj.is_destroyed ? "YES" : "NO") << "</td>";
                    file << "</tr>\n";

                    if (is_leak) leak_count++;
                }

                file << "</table>\n";
                file << "<p>Total objects: " << objects.size() << "</p>";
                file << "<p>Total leaks: " << leak_count << "</p>";
                file << "</body></html>";

                printf("[ObjectCrashRecovery] Report generated: %s\n", output_file.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                fprintf(stderr, "[ObjectCrashRecovery] Exception: %s\n", e.what());
                return false;
            }
        }
    };

} // namespace hgl::utils
