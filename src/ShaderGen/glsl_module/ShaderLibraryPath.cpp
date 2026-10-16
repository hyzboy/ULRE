#include <hgl/mtl/ShaderLibraryPath.h>
#include "../../Tools/GLSLCompiler/RootIncludePath.h"

namespace hgl::graph::mtl
{
    OSString ResolveShaderLibraryModulePath(
        const OSString &root, const AnsiString &include_path)
    {
        if (root.IsEmpty() || include_path.IsEmpty())
            return {};

        AnsiString relative = include_path;
        if (!hgl::shader::NormalizeRootIncludePath(relative.c_str()))
            return {};
        relative.Resize(hgl::strlen(relative.c_str()));
        const OSString candidate = filesystem::NormalizeFilename(
            (filesystem::Path(root) / ToOSString(relative.c_str())).ToOSString());
        if (filesystem::FileExist(candidate))
            return candidate;
        return {};
    }
}
