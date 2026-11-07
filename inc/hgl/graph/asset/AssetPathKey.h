#pragma once

/**
 * 资产路径比较键。
 *
 * 同一个资产文件在同一个进程里常常有多种写法：调用方手拼的绝对路径
 * （`E:\proj\res/a.recipe.toml`）、目录枚举出来的相对路径
 * （`res\a.recipe.toml`），分隔符还各写各的。按原始字符串比较路径因此
 * 不可靠 —— 注册表「按路径查找」会漏，异步车道「按路径去重」也会漏，
 * 结果就是同一批文件被解析两遍、再被注册表按重名拒一遍。
 *
 * `MakeAssetPathKey()` 把这些写法折叠成同一个比较键：
 *   1. 统一分隔符、合并连续分隔符（`filesystem::NormalizeFilename`）；
 *   2. 相对路径拼上当前工作目录，转成绝对路径。
 *
 * 只做字符串处理（取一次当前目录），不访问目标文件，也不解析 `.` / `..`
 * 这类点段 —— 它服务于「比较」，不服务于「打开」。
 */

#include <hgl/type/String.h>
#include <hgl/filesystem/Filename.h>
#include <hgl/filesystem/FileSystem.h>

namespace hgl::graph
{
    /// 是否已是绝对路径（盘符 / 前导分隔符）；不做磁盘访问
    inline bool IsAbsoluteAssetPath(const OSString &path)
    {
        if (path.IsEmpty())
            return false;

        if (path.Length() >= 2 && path[1] == OS_TEXT(':'))
            return true;

        return path[0] == OS_TEXT('/') || path[0] == OS_TEXT('\\');
    }

    /// 生成同文件同键的路径比较键（见文件头注释）
    inline OSString MakeAssetPathKey(const OSString &path)
    {
        if (path.IsEmpty())
            return path;

        OSString key = hgl::filesystem::NormalizeFilename(path);

        if (IsAbsoluteAssetPath(key))
            return key;

        OSString cwd;

        if (!hgl::filesystem::GetCurrentPath(cwd) || cwd.IsEmpty())
            return key;             ///< 取不到工作目录就退回规范化后的原样（仍然比原串稳定）

        return hgl::filesystem::NormalizeFilename(
            hgl::filesystem::JoinPathWithFilename(cwd, key));
    }
}//namespace hgl::graph
