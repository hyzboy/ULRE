#pragma once

namespace hgl::graph::mtl {}

#include <hgl/filesystem/FileSystem.h>
#include <hgl/type/String.h>
#include <cstdlib>

namespace hgl::graph::mtl
{
    /// 环境变量：覆盖默认 SPV 磁盘缓存根目录
    inline constexpr const wchar_t *kShaderCachePathEnvironmentVariable =
        L"ULRE_SHADER_CACHE_PATH";

    /// SPV 磁盘缓存根目录解析。
    /// 运行时（ShaderProgramManager）与离线（ShaderCooker）必须同源——否则
    /// cook 产物与运行时缓存互不可见。优先级：
    ///   环境变量 ULRE_SHADER_CACHE_PATH > exe 所在目录 > cwd。
    /// GetCurrentProgramPath 返回的已是程序目录（内部去除文件名），直接使用。
    /// 空返回值 = 无法解析，store 读写安全失败（等价无缓存，不阻断材质创建）。
    inline OSString GetShaderCacheRootPath()
    {
        const wchar_t *env_value = _wgetenv(kShaderCachePathEnvironmentVariable);
        if (env_value && env_value[0])
            return OSString(env_value);

        OSString program_path;
        if (filesystem::GetCurrentProgramPath(program_path))
            return program_path;

        OSString current_path;
        if (filesystem::GetCurrentPath(current_path))
            return current_path;

        return OSString();
    }
}
