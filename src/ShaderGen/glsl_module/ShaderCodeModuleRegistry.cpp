#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/ShaderCodeModuleMetadata.h>
#include <hgl/mtl/ShaderLibraryPath.h>

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

    bool ShaderCodeModuleRegistry::Register(const ShaderCodeModuleDefinition &definition)
    {
        if (!IsValidShaderCodeModuleDefinition(definition))
            return false;

        if (FindByName(definition.name))
            return false;

        modules.push_back(&definition);
        return true;
    }

    bool ShaderCodeModuleRegistry::LoadDirectory(const OSString &directory,
                                               int *out_file_count,
                                               int *out_error_count)
    {
        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;

        const int scan_count = hgl::filesystem::GetFileInfoList(file_list, directory, true, true, true);
        if (scan_count < 0)
        {
            GLogError(u8"[ShaderCodeModuleRegistry] Failed to scan code-module directory: %s",
                      directory.c_str());
            return false;
        }

        int file_count = 0;
        int error_count = 0;
        const int first_new_file_index = file_data.GetCount();

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

            ShaderCodeModuleFileData *data = file_data.Create();
            if (!data)
            {
                ++error_count;
                continue;
            }

            if (!LoadFileContent(full_name, data->glsl_code))
            {
                GLogError(u8"[ShaderCodeModuleRegistry] Failed to read code-module file: %s",
                          full_name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            const ShaderCodeModuleParseResult result =
                ParseShaderCodeModuleFile(data->glsl_code.c_str(), data->glsl_code.Length(), *data);

            if (result == ShaderCodeModuleParseResult::Skipped)
            {
                file_data.DeleteAt(file_data.GetCount() - 1);
                continue;
            }

            if (result != ShaderCodeModuleParseResult::OK)
            {
                GLogError(u8"[ShaderCodeModuleRegistry] Code-module metadata parse failed: file=%s error=%s",
                          full_name.c_str(), GetShaderCodeModuleParseResultName(result));
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            if (data->name.IsEmpty())
            {
                // @ulre name 是必填身份——模块身份统一为 name（T2 起），
                // 不接受"文件名即名字"的隐式特例
                GLogError(u8"[ShaderCodeModuleRegistry] Code-module metadata missing @ulre name: file=%s",
                          full_name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            if (FindByName(data->name.c_str()))
            {
                GLogError(u8"[ShaderCodeModuleRegistry] Duplicate code-module name: %s (file=%s)",
                          data->name.c_str(), full_name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            // Finalize the definition. Pointers reference stable members of the
            // heap-allocated entry, which are not mutated afterwards.
            data->definition.name = data->name.c_str();
            data->definition.glsl_code = data->glsl_code.c_str();
            data->definition.kind = data->kind;
            data->definition.priority = data->priority;
            data->definition.flags = data->flags;
            data->definition.slot_role = data->slot_role;
            data->definition.provided_capabilities =
                data->provided_capabilities;
            data->definition.required_capabilities =
                data->required_capabilities;
            data->definition.semantic_requirements = data->semantic_requirements.GetData();
            data->definition.semantic_requirement_count = static_cast<uint32>(data->semantic_requirements.GetCount());
            data->definition.semantic_provides = data->semantic_provides.GetData();
            data->definition.semantic_provide_count = static_cast<uint32>(data->semantic_provides.GetCount());
            for (int k = 0; k < data->ssbo_requirements.GetCount(); ++k)
            {
                data->ssbo_requirements[k].name = data->ssbo_name_storage[k]->c_str();
            }
            data->definition.ssbo_requirements = data->ssbo_requirements.GetData();
            data->definition.ssbo_requirement_count = static_cast<uint32>(data->ssbo_requirements.GetCount());
            data->definition.texture_layer_requirements = data->texture_layer_requirements.GetData();
            data->definition.texture_layer_requirement_count =
                static_cast<uint32>(data->texture_layer_requirements.GetCount());
            data->metadata_resolution_valid = false;

            if (!Register(data->definition))
            {
                GLogError(u8"[ShaderCodeModuleRegistry] Failed to register code module: %s",
                          data->name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            ++file_count;
        }

        // Pass 2: validate `uses <module-name>` references against the
        // now-complete registry and store dependency/conflict names.
        for (int i = 0; i < file_data.GetCount(); ++i)
        {
            ShaderCodeModuleFileData *data = file_data[i];
            if (!data || data->metadata_resolution_valid)
                continue;

            data->dependencies.Clear();
            data->module_conflict_names.Clear();
            data->metadata_resolution_valid = true;
            const int pending_count = data->pending_module_requirements.GetCount();
            for (int k = 0; k < pending_count; ++k)
            {
                const AnsiString &dependency_name = data->pending_module_requirements[k];
                const ShaderCodeModuleDefinition *dependency = FindByName(dependency_name.c_str());
                if (!dependency)
                {
                    GLogError(u8"[ShaderCodeModuleRegistry] Unresolved code-module dependency: module=%s depends_on=%s",
                              data->name.c_str(), dependency_name.c_str());
                    ++error_count;
                    data->metadata_resolution_valid = false;
                    continue;
                }

                ShaderCodeModuleDependency resolved_dependency{};
                resolved_dependency.module_name = dependency->name;
                data->dependencies.Add(resolved_dependency);
            }

            data->definition.dependencies = data->dependencies.GetData();
            data->definition.dependency_count =
                static_cast<uint32>(data->dependencies.GetCount());

            for (int k = 0;
                 k < data->pending_module_conflicts.GetCount();
                 ++k)
            {
                const AnsiString &conflict_name =
                    data->pending_module_conflicts[k];
                const ShaderCodeModuleDefinition *conflict =
                    FindByName(conflict_name.c_str());
                if (!conflict)
                {
                    GLogError(u8"[ShaderCodeModuleRegistry] Unresolved code-module conflict: module=%s conflicts_with=%s",
                              data->name.c_str(), conflict_name.c_str());
                    ++error_count;
                    data->metadata_resolution_valid = false;
                    continue;
                }

                data->module_conflict_names.Add(conflict->name);
            }

            data->definition.module_conflict_names =
                data->module_conflict_names.GetData();
            data->definition.module_conflict_count =
                static_cast<uint32>(data->module_conflict_names.GetCount());
        }

        for (int i = 0; i < file_data.GetCount(); ++i)
        {
            ShaderCodeModuleFileData *data = file_data[i];
            if (!data || data->metadata_resolution_valid)
                continue;

            if (RemoveByName(data->name.c_str())
             && i >= first_new_file_index)
                --file_count;
        }

        bool removed_incomplete_module = true;
        while (removed_incomplete_module)
        {
            removed_incomplete_module = false;

            for (int i = 0; i < file_data.GetCount(); ++i)
            {
                ShaderCodeModuleFileData *data = file_data[i];
                if (!data || !FindByName(data->name.c_str()))
                    continue;

                bool complete =
                    data->metadata_resolution_valid
                 && data->dependencies.GetCount()
                        == data->pending_dependency_versions.GetCount()
                 && data->module_conflict_names.GetCount()
                        == data->pending_module_conflicts.GetCount();
                for (int k = 0;
                     complete
                        && k < data->dependencies.GetCount();
                     ++k)
                {
                    if (!FindByName(data->dependencies[k].module_name))
                    {
                        complete = false;
                        break;
                    }
                }

                if (complete)
                {
                    for (int k = 0;
                         k < data->module_conflict_names.GetCount();
                         ++k)
                    {
                        if (!FindByName(data->module_conflict_names[k]))
                        {
                            complete = false;
                            break;
                        }
                    }
                }

                if (!complete && RemoveByName(data->name.c_str()))
                {
                    data->metadata_resolution_valid = false;
                    if (i >= first_new_file_index)
                        --file_count;
                    ++error_count;
                    removed_incomplete_module = true;
                }
            }
        }

        // 完整校验接线到生产加载收尾（DFS 环检测 / AmbiguousProviderPriority）：
        // 此前校验层仅回归门使用，pass2 只有散装检查；此处补全生产侧约束。
        ShaderCodeModuleMetadataValidationDiagnostic validation_diagnostic{};
        if (!ValidateShaderCodeModuleRegistryMetadata(
                *this, validation_diagnostic))
        {
            GLogError(u8"[ShaderCodeModuleRegistry] Metadata validation failed: "
                      u8"module=%s related=%s error=%u",
                      validation_diagnostic.module_name.c_str(),
                      validation_diagnostic.related_module_name.c_str(),
                      static_cast<uint32>(validation_diagnostic.error));
            ++error_count;
        }

        if (out_file_count)
            *out_file_count = file_count;
        if (out_error_count)
            *out_error_count = error_count;

        GLogInfo(u8"[ShaderCodeModuleRegistry] Loaded %d code modules from %s (%d errors)",
                 file_count, directory.c_str(), error_count);

        return true;
    }

    const ShaderCodeModuleDefinition *ShaderCodeModuleRegistry::FindByName(const char *name) const
    {
        if (!name || !*name)
            return nullptr;

        for (const auto *definition : modules)
        {
            if (definition && definition->name && std::strcmp(definition->name, name) == 0)
                return definition;
        }

        return nullptr;
    }

    bool ShaderCodeModuleRegistry::RemoveByName(const char *name)
    {
        if (!name || !*name)
            return false;

        for (size_t i = 0; i < modules.size(); ++i)
        {
            const auto *definition = modules[i];
            if (definition && definition->name && std::strcmp(definition->name, name) == 0)
            {
                modules.erase(modules.begin() + static_cast<ptrdiff_t>(i));
                return true;
            }
        }

        return false;
    }

    const ShaderCodeModuleDefinition *ShaderCodeModuleRegistry::GetModuleByIndex(const int index) const
    {
        const int count = static_cast<int>(modules.size());
        if (index < 0 || index >= count)
            return nullptr;

        return modules[index];
    }

    ShaderCodeModuleRegistry &GetShaderCodeModuleRegistry()
    {
        static ShaderCodeModuleRegistry registry;
        static bool loaded = false;
        if (!loaded)
        {
            registry.LoadDirectory(ToOSString(mtl::GetShaderLibraryPath()));
            loaded = true;
        }
        return registry;
    }
}
