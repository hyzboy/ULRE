#pragma once

namespace hgl::graph::mtl {}

#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>
#include <string>
#include <vector>
#include <cstdlib>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

    /// 环境变量覆盖项：优先级高于 requested_path 与向上搜索。
    /// 指向的目录必须本身是 ShaderLibrary 根（含 material/ 子目录）。
    inline constexpr const wchar_t *kShaderLibraryPathEnvironmentVariable =
        L"ULRE_SHADERLIBRARY_PATH";

    inline std::string ShaderLibraryPathToUTF8(const OSString &path)
    {
        const U8String utf8 = ToU8String(path);
        return std::string(
            reinterpret_cast<const char *>(utf8.c_str()),
            utf8.Length());
    }

    inline bool IsShaderLibraryRoot(const hgl::filesystem::Path &path)
    {
        return path.IsDirectory()
            && (path / OSString(OS_TEXT("material"))).IsDirectory();
    }

    inline std::string GetShaderLibraryPath(const char *requested_path = nullptr)
    {
        // Resolved candidates visited across every lookup stage, reported in
        // full when the library root cannot be found.
        std::vector<std::string> searched;

        // 1. Environment variable override (highest priority).
        const wchar_t *env_value = _wgetenv(kShaderLibraryPathEnvironmentVariable);
        if (env_value && env_value[0] != L'\0')
        {
            const hgl::filesystem::Path env_root(env_value);
            searched.push_back(ShaderLibraryPathToUTF8(env_value));
            if (IsShaderLibraryRoot(env_root))
                return ShaderLibraryPathToUTF8(env_value);

            GLogWarning(
                "[ShaderGen] ULRE_SHADERLIBRARY_PATH points to a non-ShaderLibrary root: %s (ignored)",
                ShaderLibraryPathToUTF8(env_value).c_str());
        }

        // 2. Explicit request.
        const OSString requested = ToOSString(requested_path ? requested_path : "");
        const hgl::filesystem::Path requested_root(requested);
        if (!requested.IsEmpty() && IsShaderLibraryRoot(requested_root))
            return ShaderLibraryPathToUTF8(requested);

        const OSString relative_name = requested.IsEmpty()
            ? OSString(OS_TEXT("ShaderLibrary"))
            : requested;

        auto search_from = [&](const OSString &start)
        {
            hgl::filesystem::Path current(start);
            for (uint32 depth = 0; depth < 64 && !current.IsEmpty(); ++depth)
            {
                const hgl::filesystem::Path candidate = current / relative_name;
                searched.push_back(ShaderLibraryPathToUTF8(candidate));
                if (IsShaderLibraryRoot(candidate))
                    return std::string(ShaderLibraryPathToUTF8(candidate));

                const hgl::filesystem::Path parent = current.GetParent();
                if (parent == current)
                    break;
                current = parent;
            }
            return std::string();
        };

        OSString program_path;
        if (hgl::filesystem::GetCurrentProgramPath(program_path))
        {
            const std::string found = search_from(program_path);
            if (!found.empty())
                return found;
        }

        OSString current_path;
        if (hgl::filesystem::GetCurrentPath(current_path))
        {
            const std::string found = search_from(current_path);
            if (!found.empty())
                return found;
        }

        // 3. Failure: report the complete search chain.
        std::string chain;
        for (const std::string &candidate : searched)
        {
            if (!chain.empty())
                chain += "; ";
            chain += candidate;
        }
        GLogError(
            "[ShaderGen] ShaderLibrary root not found. Set ULRE_SHADERLIBRARY_PATH to the ShaderLibrary directory "
            "(or run from a location that contains it). Searched: %s",
            chain.empty() ? "<none>" : chain.c_str());
        return requested_path && requested_path[0]
            ? requested_path
            : "ShaderLibrary";
    }
}
