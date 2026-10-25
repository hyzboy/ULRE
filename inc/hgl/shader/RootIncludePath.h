#pragma once

// 根相对 include 路径的归一化与判定。
//
// 归属说明：本文件曾位于 src/Tools/GLSLCompiler/RootIncludePath.h。那个目录是
// 独立 submodule（GLSLCompiler），必须能脱离 ULRE 单独构建，因此不能反向依赖
// inc/ 下的公共头；而 ULRE.ShaderProgramBuild 是库，也不该跨仓库去 include 工具侧的私有
// 头。于是两侧各持一份实现，均为无状态 inline 函数。
// 约束：两份实现必须保持逐字一致（同名同 namespace 的 inline 函数若在某 TU 内
// 同时可见且不一致，属于 ODR 违反）。修改时请同步
// src/Tools/GLSLCompiler/RootIncludePath.h。

namespace hgl::shader
{
    inline bool IsIncludeSeparator(const char ch)
    {
        return ch == '/' || ch == '\\';
    }

    // Task-generated sources live under a per-job directory instead of the shader
    // library root; every other root-relative include resolves against the library.
    // Normalize first: `generated/../common/x.glsl` collapses to `common/x.glsl`
    // and must not be able to hop over the partition.
    inline bool HasGeneratedPrefix(const char *path)
    {
        static constexpr char kPrefix[] = "generated/";
        if (!path)
            return false;
        for (int i = 0; kPrefix[i]; ++i)
            if (path[i] != kPrefix[i])
                return false;
        return true;
    }

    // Normalize in place without permitting absolute paths or escape above root.
    inline bool NormalizeRootIncludePath(char *path)
    {
        if (!path || !*path || IsIncludeSeparator(*path))
            return false;

        char *source = path;
        char *target = path;
        while (*source)
        {
            const char *part = source;
            while (*source && !IsIncludeSeparator(*source))
            {
                if (*source == ':')
                    return false;
                ++source;
            }
            const auto length = source - part;
            while (IsIncludeSeparator(*source))
                ++source;

            if (length == 1 && part[0] == '.')
                continue;
            if (length == 2 && part[0] == '.' && part[1] == '.')
            {
                if (target == path)
                    return false;
                while (target > path && target[-1] != '/')
                    --target;
                if (target > path)
                    --target;
            }
            else if (length > 0)
            {
                if (target > path)
                    *target++ = '/';
                for (auto i = 0; i < length; ++i)
                    *target++ = part[i];
            }
        }
        *target = 0;
        return target > path;
    }
}
