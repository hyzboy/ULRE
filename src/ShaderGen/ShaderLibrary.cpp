#include <string>
#include <unordered_map>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/io/LoadString.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/Filesystem.h>

namespace hgl::graph::mtl{

namespace
{
    std::unordered_map<std::string, std::string> shader_library;
}

// 因为是Debug阶段，所以现在直接从文件系统加载

const std::string *LoadShader(const std::string &shader_name)
{
    if(shader_name.empty())
        return(nullptr);

    // Check cache
    auto it = shader_library.find(shader_name);
    if (it != shader_library.end())
        return &it->second;

    const std::string filename=shader_name+".glsl";

    const std::string fullname=filesystem::JoinPathWithFilename("ShaderLibrary",filename);

    const OSString os_fn=ToOSString(fullname);

    if(!filesystem::FileExist(os_fn))
        return(nullptr);

    std::string loaded_shader;

    if(LoadStringFromTextFile((U8String &)loaded_shader,os_fn)<=0)
    {
        return nullptr;
    }

    auto [insert_it, ok] = shader_library.emplace(shader_name, std::move(loaded_shader));
    return &insert_it->second;
}

}//namespace hgl::graph::mtl
