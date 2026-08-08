#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>
#include <hgl/utf.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        bool IsGLSLFile(const OSString &file_name)
        {
            return file_name.EndsWith(OS_TEXT(".glsl"));
        }

        bool LoadFileContent(const OSString &path, AnsiString &out_code)
        {
            hgl::io::OpenFileInputStream opener(path);
            if (!opener)
                return false;

            const int64 size = opener->GetSize();
            if (size <= 0)
                return false;

            hgl::AutoDeleteArray<char> buffer(size_t(size) + 1);
            if (!buffer)
                return false;

            if (opener->Read(static_cast<void *>(buffer.data()), size) != size)
                return false;

            buffer[size_t(size)] = 0;
            out_code = AnsiString(buffer.data(), int(size));
            return true;
        }
    }

    bool GLSLCodeModuleRegistry::Register(const GLSLCodeModuleDefinition &definition)
    {
        if (!IsValidGLSLCodeModuleDefinition(definition))
            return false;

        if (modules.ContainsKey(definition.id))
            return false;

        return modules.Add(definition.id, &definition);
    }

    bool GLSLCodeModuleRegistry::RegisterBuiltinModules()
    {
        for (uint32 i = 0; i < static_cast<uint32>(GLSLCodeModuleID::RANGE_SIZE); ++i)
        {
            const auto id = static_cast<GLSLCodeModuleID>(i);
            const auto *definition = FindGLSLCodeModuleDefinition(id);
            if (!definition || !Register(*definition))
                return false;
        }

        return true;
    }

    bool GLSLCodeModuleRegistry::LoadDirectory(const OSString &directory,
                                               int *out_file_count,
                                               int *out_error_count)
    {
        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;

        const int scan_count = hgl::filesystem::GetFileInfoList(file_list, directory, true, true, true);
        if (scan_count < 0)
        {
            GLogError(u8"[GLSLCodeModuleRegistry] Failed to scan code-module directory: %s",
                      directory.c_str());
            return false;
        }

        int file_count = 0;
        int error_count = 0;

        // Pass 1: read, parse and register every file-backed module.
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &file_info = file_list[i];
            if (!file_info.is_file)
                continue;

            const OSString file_name(file_info.name);
            if (!IsGLSLFile(file_name))
                continue;

            const OSString full_name(file_info.fullname);

            GLSLCodeModuleFileData *data = file_data.Create();
            if (!data)
            {
                ++error_count;
                continue;
            }

            if (!LoadFileContent(full_name, data->glsl_code))
            {
                GLogError(u8"[GLSLCodeModuleRegistry] Failed to read code-module file: %s",
                          full_name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            const GLSLCodeModuleParseResult result =
                ParseGLSLCodeModuleFile(data->glsl_code.c_str(), data->glsl_code.Length(), *data);

            if (result == GLSLCodeModuleParseResult::Skipped)
            {
                file_data.DeleteAt(file_data.GetCount() - 1);
                continue;
            }

            if (result != GLSLCodeModuleParseResult::OK)
            {
                GLogError(u8"[GLSLCodeModuleRegistry] Code-module metadata parse failed: file=%s error=%s",
                          full_name.c_str(), GetGLSLCodeModuleParseResultName(result));
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            if (data->name.IsEmpty())
            {
                // 文件内无 name 指令时，用文件名（去目录、去扩展名）作为默认模块名
                const OSString file_name(full_name);
                int name_start = 0;
                const int slash = file_name.FindRightChar(HGL_DIRECTORY_SEPARATOR);
                if (slash >= 0)
                    name_start = slash + 1;

                const OSString base = file_name.SubString(name_start);
                const int dot = base.FindRightChar(OS_TEXT('.'));
                const OSString stem = (dot >= 0) ? base.SubString(0, dot) : base;

                const U8String stem_u8 = hgl::ToU8String(stem);
                data->name = AnsiString(reinterpret_cast<const char *>(stem_u8.c_str()),
                                        stem_u8.Length());
            }

            if (FindByName(data->name.c_str()))
            {
                GLogError(u8"[GLSLCodeModuleRegistry] Duplicate code-module name: %s (file=%s)",
                          data->name.c_str(), full_name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            // Finalize the definition. Pointers reference stable members of the
            // heap-allocated entry, which are not mutated afterwards.
            data->definition.id = static_cast<GLSLCodeModuleID>(next_file_id++);
            data->definition.name = data->name.c_str();
            data->definition.glsl_code = data->glsl_code.c_str();
            data->definition.kind = data->kind;
            data->definition.priority = data->priority;
            data->definition.flags = data->flags;
            data->definition.semantic_requirements = data->semantic_requirements.GetData();
            data->definition.semantic_requirement_count = static_cast<uint32>(data->semantic_requirements.GetCount());
            data->definition.semantic_provides = data->semantic_provides.GetData();
            data->definition.semantic_provide_count = static_cast<uint32>(data->semantic_provides.GetCount());
            data->definition.ubo_requirements = data->ubo_requirements.GetData();
            data->definition.ubo_requirement_count = static_cast<uint32>(data->ubo_requirements.GetCount());
            for (int k = 0; k < data->ssbo_requirements.GetCount(); ++k)
            {
                data->ssbo_requirements[k].name = data->ssbo_name_storage[k]->c_str();
            }
            data->definition.ssbo_requirements = data->ssbo_requirements.GetData();
            data->definition.ssbo_requirement_count = static_cast<uint32>(data->ssbo_requirements.GetCount());
            for (int k = 0; k < data->texture_requirements.GetCount(); ++k)
            {
                data->texture_requirements[k].name = data->texture_name_storage[k]->c_str();
                data->texture_requirements[k].glsl_type = data->texture_type_storage[k]->c_str();
            }
            data->definition.texture_requirements = data->texture_requirements.GetData();
            data->definition.texture_requirement_count = static_cast<uint32>(data->texture_requirements.GetCount());
            data->definition.texture_layer_requirements = data->texture_layer_requirements.GetData();
            data->definition.texture_layer_requirement_count =
                static_cast<uint32>(data->texture_layer_requirements.GetCount());

            if (!Register(data->definition))
            {
                GLogError(u8"[GLSLCodeModuleRegistry] Failed to register code module: %s",
                          data->name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            ++file_count;
        }

        // Pass 2: resolve `uses <module-name>` references against the
        // now-complete registry.
        for (int i = 0; i < file_data.GetCount(); ++i)
        {
            GLSLCodeModuleFileData *data = file_data[i];
            if (!data)
                continue;

            const int pending_count = data->pending_module_requirements.GetCount();
            if (pending_count == 0)
                continue;

            for (int k = 0; k < pending_count; ++k)
            {
                const AnsiString &dependency_name = data->pending_module_requirements[k];
                const GLSLCodeModuleDefinition *dependency = FindByName(dependency_name.c_str());
                if (!dependency)
                {
                    GLogError(u8"[GLSLCodeModuleRegistry] Unresolved code-module dependency: module=%s depends_on=%s",
                              data->name.c_str(), dependency_name.c_str());
                    ++error_count;
                    continue;
                }

                data->code_module_requirements.Add(dependency->id);
            }

            data->definition.code_module_requirements = data->code_module_requirements.GetData();
            data->definition.code_module_requirement_count = static_cast<uint32>(data->code_module_requirements.GetCount());
        }

        if (out_file_count)
            *out_file_count = file_count;
        if (out_error_count)
            *out_error_count = error_count;

        GLogInfo(u8"[GLSLCodeModuleRegistry] Loaded %d code modules from %s (%d errors)",
                 file_count, directory.c_str(), error_count);

        return true;
    }

    const GLSLCodeModuleDefinition *GLSLCodeModuleRegistry::Find(const GLSLCodeModuleID id) const
    {
        const GLSLCodeModuleDefinition *definition = nullptr;
        modules.Get(id, definition);
        return definition;
    }

    const GLSLCodeModuleDefinition *GLSLCodeModuleRegistry::FindByName(const char *name) const
    {
        if (!name || !*name)
            return nullptr;

        for (const auto &entry : modules)
        {
            const auto *definition = entry.second;
            if (definition && definition->name && std::strcmp(definition->name, name) == 0)
                return definition;
        }

        return nullptr;
    }

    const GLSLCodeModuleDefinition *GLSLCodeModuleRegistry::GetModuleByIndex(const int index) const
    {
        const int count = static_cast<int>(modules.GetCount());
        if (index < 0 || index >= count)
            return nullptr;

        auto it = modules.begin();
        for (int i = 0; i < index; ++i)
            ++it;

        return it->second;
    }
}
