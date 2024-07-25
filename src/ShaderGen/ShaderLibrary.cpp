#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/io/LoadString.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/Filesystem.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    ObjectMap<AnsiString,AnsiString> shader_library;
}

// 因为是Debug阶段，所以现在直接从文件系统加载

const AnsiString *LoadShader(const AnsiString &shader_name)
{
    if(shader_name.IsEmpty())
        return(nullptr);

    AnsiString *shader;

    if(shader_library.Get(shader_name,shader))
        return shader;

    const AnsiString filename=shader_name+".glsl";

    const AnsiString fullname=filesystem::MergeFilename("ShaderLibrary",filename);

    const OSString os_fn=ToOSString(fullname);

    if(!filesystem::FileExist(os_fn))
        return(nullptr);

    shader=new AnsiString;

    if(LoadStringFromTextFile((UTF8String &)*shader,os_fn)<=0)
    {
        delete shader;
        shader=nullptr;
    }

    shader_library.Add(shader_name,shader);

    return shader;
}

STD_MTL_NAMESPACE_END
