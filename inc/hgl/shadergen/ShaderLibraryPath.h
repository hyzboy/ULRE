#pragma once

namespace hgl::graph::mtl {}

#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>
#include <string>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
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
        const OSString requested = ToOSString(requested_path ? requested_path : "");
        const hgl::filesystem::Path requested_root(requested);
        if (!requested.IsEmpty() && IsShaderLibraryRoot(requested_root))
            return ShaderLibraryPathToUTF8(requested);

        const OSString relative_name = requested.IsEmpty()
            ? OSString(OS_TEXT("ShaderLibrary"))
            : requested;

        auto search_from = [&](const OSString &start) -> std::string
        {
            hgl::filesystem::Path current(start);
            for (uint32 depth = 0; depth < 64 && !current.IsEmpty(); ++depth)
            {
                const hgl::filesystem::Path candidate = current / relative_name;
                if (IsShaderLibraryRoot(candidate))
                    return ShaderLibraryPathToUTF8(candidate);

                const hgl::filesystem::Path parent = current.GetParent();
                if (parent == current)
                    break;
                current = parent;
            }
            return {};
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

        GLogWarning("[ShaderGen] ShaderLibrary root not found; requested=%s",
                    requested_path ? requested_path : "<default>");
        return requested_path && requested_path[0]
            ? requested_path
            : "ShaderLibrary";
    }
}
