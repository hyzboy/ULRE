#pragma once

#include <hgl/mtl/ShaderLibraryPack.h>
#include <hgl/log/Log.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    /**
     * 按 ShaderLibrary 包（挂载名 `ShaderLibrary`）读入一份 GLSL/compute 源文本。
     *
     * - 路径单源：统一经 `ReadShaderLibraryText()` 走包内访问（未挂载时回退物理根目录）。
     *   示例里不写任何绝对路径、也不拼 `../`。
     * - 只读**开发形态**产物（发行包不带 .glsl）；失败返回空串并记一条错误日志。
     *
     * 用途：把「直接源码」路径（`CreateComputePipeline(name, glsl, pc_size)`）需要的
     * GLSL 从 C++ 字符串字面量搬到 `ShaderLibrary/compute/*.glsl` —— 改 shader 不必重编译。
     * 这类文件不带模块指令，`GlslModuleRegistry` 扫描整个 ShaderLibrary 时会按
     * Skipped 跳过，不会被当成代码模块。
     */
    inline AnsiString LoadShaderLibrarySourceText(const char *relative_path)
    {
        if (!relative_path || !relative_path[0])
            return AnsiString();

        AnsiString text;
        if (!ReadShaderLibraryText(ToOSString(relative_path), text) || text.IsEmpty())
        {
            GLogError(u8"[ShaderLibrarySource] 读取 shader 源失败（发行形态不带 .glsl）：%s",
                      relative_path);
            return AnsiString();
        }

        return text;
    }
}//namespace hgl::graph::mtl
