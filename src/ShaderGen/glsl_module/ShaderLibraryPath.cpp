#include <hgl/mtl/ShaderLibraryPath.h>

namespace hgl::graph::mtl
{
    OSString ResolveShaderLibraryModulePath(
        const OSString &root, const AnsiString &include_path)
    {
        if (root.IsEmpty() || include_path.IsEmpty())
            return {};

        AnsiString input = include_path;
        input.Replace('\\', '/');
        if (input[0] == '/' || input.FindChar(':') >= 0)
            return {};

        for (int search_index = 0; search_index < 2; ++search_index)
        {
            // Normalize after adding the search prefix: common/../x must
            // resolve exactly as the compiler's second include directory.
            AnsiString relative = search_index == 0
                ? input : AnsiString("common/") + input;
            AnsiString normalized;
            for (int start = 0; start <= relative.Length();)
            {
                int end = start;
                while (end < relative.Length() && relative[end] != '/')
                    ++end;
                const AnsiString part = relative.SubString(start, end - start);
                if (part == "..")
                {
                    if (normalized.IsEmpty())
                        break;
                    const int separator = normalized.FindRightChar('/');
                    normalized = separator < 0
                        ? AnsiString() : normalized.SubString(0, separator);
                }
                else if (!part.IsEmpty() && part != ".")
                {
                    if (!normalized.IsEmpty())
                        normalized += "/";
                    normalized += part;
                }
                if (end == relative.Length())
                {
                    const OSString candidate = filesystem::NormalizeFilename(
                        (filesystem::Path(root)
                            / ToOSString(normalized.c_str())).ToOSString());
                    if (!normalized.IsEmpty() && filesystem::FileExist(candidate))
                        return candidate;
                    break;
                }
                start = end + 1;
            }
        }
        return {};
    }
}
