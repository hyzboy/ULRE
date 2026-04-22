#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <fstream>
#include <sstream>

namespace hgl::graph::internal {

size_t SkipBOMAndLeadingWhitespace(const std::string &source)
{
    if (source.empty())
        return 0;

    size_t begin = 0;

    // UTF-8 BOM: EF BB BF
    if (source.size() >= 3
    && static_cast<unsigned char>(source[0]) == 0xEF
    && static_cast<unsigned char>(source[1]) == 0xBB
    && static_cast<unsigned char>(source[2]) == 0xBF)
    {
        begin = 3;
    }

    while (begin < source.size())
    {
        const char ch = source[begin];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            ++begin;
        else
            break;
    }

    return begin;
}

size_t FindVersionDirectiveLineEnd(const std::string &source)
{
    if (source.empty())
        return std::string::npos;

    const size_t begin = SkipBOMAndLeadingWhitespace(source);

    if (begin + 8 <= source.size() && source.compare(begin, 8, "#version") == 0)
    {
        const size_t eol = source.find('\n', begin);
        return eol == std::string::npos ? source.size() : (eol + 1);
    }

    return std::string::npos;
}

std::string InjectAfterVersion(const std::string &source, const std::string &injection)
{
    if (injection.empty())
        return source;

    const size_t insert_pos = FindVersionDirectiveLineEnd(source);
    if (insert_pos != std::string::npos)
    {
        std::string result;
        result.reserve(source.size() + injection.size() + 2);
        result.append(source, 0, insert_pos);
        result.append("\n");
        result.append(injection);
        result.append("\n");
        result.append(source, insert_pos, std::string::npos);
        return result;
    }

    // No #version line: prepend
    return injection + "\n" + source;
}

std::string BuildGLSLPreviewFirstLines(const std::string &source, const size_t max_lines)
{
    if (source.empty() || max_lines == 0)
        return std::string();

    size_t line_count = 0;
    size_t pos = 0;

    while (pos < source.size() && line_count < max_lines)
    {
        const size_t eol = source.find('\n', pos);
        ++line_count;

        if (eol == std::string::npos)
            return source;

        pos = eol + 1;
    }

    if (pos >= source.size())
        return source;

    return source.substr(0, pos);
}

bool ReadTextFile(const std::string &path, std::string &out_content, std::string &out_error)
{
    std::ifstream ifs(path, std::ios::in);
    if (!ifs.is_open())
    {
        out_error = "Failed to open file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    out_content = ss.str();
    return true;
}

} // namespace hgl::graph::internal
