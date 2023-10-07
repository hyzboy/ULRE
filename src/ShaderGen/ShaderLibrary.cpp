#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/io/LoadString.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/Filesystem.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    ObjectMap<AnsiString,UTF8String> shader_library;
}

// ��Ϊ��Debug�׶Σ���������ֱ�Ӵ��ļ�ϵͳ����

const AnsiString *LoadShader(const AnsiString &shader_name)
{
    if(shader_name.IsEmpty())
        return(nullptr);

    UTF8String *shader;

    if(shader_library.Get(shader_name,shader))
        return shader;

    const AnsiString filename=shader_name+".glsl";

    const AnsiString fullname=filesystem::MergeFilename("ShaderLibrary",filename);

    const OSString os_fn=ToOSString(fullname);

    if(!filesystem::FileExist(os_fn))
        return(nullptr);

    shader=new UTF8String;

    if(LoadStringFromTextFile(*shader,os_fn)<=0)
    {
        delete shader;
        shader=nullptr;
    }

    shader_library.Add(shader_name,shader);

    return shader;
}

STD_MTL_NAMESPACE_END
