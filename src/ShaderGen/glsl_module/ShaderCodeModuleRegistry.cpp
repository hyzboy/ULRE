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
        OSString source_root = filesystem::NormalizeFilename(directory);
        if (!source_root.IsEmpty()
         && source_root[0] != HGL_DIRECTORY_SEPARATOR
         && (source_root.Length() < 2 || source_root[1] != ':'))
        {
            OSString current_path;
            if (!filesystem::GetCurrentPath(current_path))
            {
                GLogError(u8"[ShaderCodeModuleRegistry] Cannot resolve scan root");
                return false;
            }
            source_root = filesystem::NormalizeFilename(
                (filesystem::Path(current_path) / source_root).ToOSString());
        }
        while (source_root.Length() > 1
            && source_root[source_root.Length() - 1] == HGL_DIRECTORY_SEPARATOR
            && !(source_root.Length() == 3 && source_root[1] == ':'))
            source_root.Delete(source_root.Length() - 1, 1);
        const int relative_start = source_root.Length()
            + (!source_root.IsEmpty()
                && source_root[source_root.Length() - 1] != HGL_DIRECTORY_SEPARATOR ? 1 : 0);

        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;

        const int scan_count = hgl::filesystem::GetFileInfoList(file_list, source_root, true, true, true);
        if (scan_count < 0)
        {
            GLogError(u8"[ShaderCodeModuleRegistry] Failed to scan code-module directory: %ls",
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
                GLogError(u8"[ShaderCodeModuleRegistry] Failed to read code-module file: %ls",
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
                GLogError(u8"[ShaderCodeModuleRegistry] Code-module metadata parse failed: file=%ls error=%s",
                          full_name.c_str(), GetShaderCodeModuleParseResultName(result));
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            if (data->name.IsEmpty())
            {
                // @ulre name 是必填身份——模块身份统一为 name（T2 起），
                // 不接受"文件名即名字"的隐式特例
                GLogError(u8"[ShaderCodeModuleRegistry] Code-module metadata missing @ulre name: file=%ls",
                          full_name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            if (FindByName(data->name.c_str()))
            {
                GLogError(u8"[ShaderCodeModuleRegistry] Duplicate code-module name: %s (file=%ls)",
                          data->name.c_str(), full_name.c_str());
                file_data.DeleteAt(file_data.GetCount() - 1);
                ++error_count;
                continue;
            }

            data->source_root = source_root;
            data->source_path = filesystem::NormalizeFilename(full_name);
            const OSString relative = data->source_path.SubString(relative_start);
            const U8String relative_utf8 = ToU8String(relative);
            data->include_path = AnsiString(
                reinterpret_cast<const char *>(relative_utf8.c_str()),
                relative_utf8.Length());
            data->include_path.Replace('\\', '/');

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
            for (int k = 0;
                 k < data->texture_reference_requirements.GetCount();
                 ++k)
            {
                data->texture_reference_requirements[k].texture_name =
                    data->texture_reference_name_storage[k]->c_str();
            }
            data->definition.texture_reference_requirements =
                data->texture_reference_requirements.GetData();
            data->definition.texture_reference_requirement_count =
                static_cast<uint32>(
                    data->texture_reference_requirements.GetCount());
            for (int k = 0;
                 k < data->sampler_reference_requirements.GetCount();
                 ++k)
            {
                data->sampler_reference_requirements[k].sampler_name =
                    data->sampler_reference_name_storage[k]->c_str();
            }
            data->definition.sampler_reference_requirements =
                data->sampler_reference_requirements.GetData();
            data->definition.sampler_reference_requirement_count =
                static_cast<uint32>(
                    data->sampler_reference_requirements.GetCount());
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

            // `@ulre uses` 是模块依赖的**唯一真源**：manifest 模块图、能力闭包、
            // 资源/采样器契约与模块冲突检查全部只沿 uses 边展开。正文里手写的
            // `#include` 若没有对应声明，该依赖模块就会整条从图里消失——它的
            // provides/require 不被计入、它声明的 texture/sampler 引用不被校验、
            // 它与其他模块的冲突不被检测（P4：provider 用 include 隐式拉入的
            // 模块正是这种情况）。因此这里强制两侧一致：正文 include 到的每个
            // 已注册模块，都必须在 uses 里自证。
            {
                AnsiStringList include_paths;
                CollectShaderCodeModuleIncludePaths(
                    data->glsl_code.c_str(), data->glsl_code.Length(), include_paths);

                for (int k = 0; k < include_paths.GetCount(); ++k)
                {
                    const ShaderCodeModuleFileData *included = nullptr;
                    for (int m = 0; m < file_data.GetCount(); ++m)
                    {
                        const ShaderCodeModuleFileData *candidate = file_data[m];
                        if (!candidate || candidate == data)
                            continue;
                        if (!FindByName(candidate->name.c_str()))
                            continue;
                        if (!(candidate->include_path == include_paths[k]))
                            continue;

                        included = candidate;
                        break;
                    }

                    // 非 ShaderLibrary 模块（例如编译器内置注入）不参与模块图。
                    if (!included)
                        continue;

                    bool declared = false;
                    for (int d = 0; d < data->pending_module_requirements.GetCount(); ++d)
                    {
                        if (data->pending_module_requirements[d] == included->name)
                        {
                            declared = true;
                            break;
                        }
                    }

                    if (declared)
                        continue;

                    GLogError(u8"[ShaderCodeModuleRegistry] Undeclared code-module dependency: module=%s includes=%s (@ulre uses %s missing)",
                              data->name.c_str(),
                              include_paths[k].c_str(),
                              included->name.c_str());
                    ++error_count;
                    data->metadata_resolution_valid = false;
                }
            }
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
                        == data->pending_module_requirements.GetCount();
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

        // 生产侧收尾校验：单条定义的自述完整性 + 同优先级 provider 歧义。
        // 缺失依赖 / 缺失冲突目标已在上面的 incomplete 剔除环里处理；
        // 依赖环由 `BuildShaderCodeResourceManifest` 的 DFS 报 `CodeModuleCycle`。
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

    const ShaderCodeModuleFileData *ShaderCodeModuleRegistry::FindFileByName(
        const char *name) const
    {
        const ShaderCodeModuleDefinition *definition = FindByName(name);
        if (!definition)
            return nullptr;
        for (int index = 0; index < file_data.GetCount(); ++index)
        {
            const ShaderCodeModuleFileData *data = file_data[index];
            if (&data->definition == definition)
                return data;
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
