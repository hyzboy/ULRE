// MeshShaderTemplate.cpp — mesh 生成器 GLSL 模板加载实现（S3）

#include "MeshShaderTemplate.h"

#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/log/Log.h>
#include <fstream>
#include <map>

namespace hgl::graph::mtl
{
namespace
{
    /// CRLF → LF：模板经 git 检出可能带 \r，归一化后生成结果与行尾策略无关
    std::string NormalizeEOL(const std::string &src)
    {
        std::string out;
        out.reserve(src.size());

        for (const char c : src)
            if (c != '\r')
                out += c;

        return out;
    }

    bool LoadTemplateFile(const std::string &path, std::string &out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;

        std::string raw;
        raw.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());

        out = NormalizeEOL(raw);
        return true;
    }
}//namespace

const std::string &GetMeshShaderTemplate(const char *filename)
{
    // 进程内缓存：模板文件在运行期不变，避免每次材质编译重复读盘
    //（用户偏好：重复查询的资源创建时缓存一次，不要每次调用重取）
    static std::map<std::string, std::string> cache;
    static const std::string empty;

    if (!filename || !filename[0])
        return empty;

    const std::string key(filename);

    const auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    const std::string path =
        GetShaderLibraryPath() + "/mesh/" + key;

    std::string text;
    if (!LoadTemplateFile(path, text))
    {
        GLogError(u8"[MeshShaderTemplate] 模板文件缺失: %s", path.c_str());
        return empty;
    }

    return cache.emplace(key, std::move(text)).first->second;
}

void ApplyMeshTemplateSlot(std::string &text, const char *slot, const std::string &value)
{
    if (!slot || !slot[0])
        return;

    const std::string marker = std::string("{{") + slot + "}}";

    size_t pos = 0;
    while ((pos = text.find(marker, pos)) != std::string::npos)
    {
        text.replace(pos, marker.size(), value);
        pos += value.size();
    }
}

void AppendMeshShaderTemplate(std::string &out, const char *filename)
{
    const std::string &text = GetMeshShaderTemplate(filename);

    if (text.empty())
    {
        // 显式失败：宁可让 glslang 报 #error，也不静默产出缺代码的 shader
        out += "#error mesh shader template missing: ";
        out += filename ? filename : "<null>";
        out += "\n";
        return;
    }

    out += text;
}

}//namespace hgl::graph::mtl
