#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/node/finished.h>
#include<hgl/graph/shader/ShaderMaker.h>

BEGIN_SHADER_NAMESPACE
bool CreateDefaultMaterial()
{
    ShaderMaker vs_maker(new node::VertexFinished());

    if(!vs_maker.Make())
        return(false);

    ShaderMaker fs_maker(new node::FragmentFinished());

    if(!fs_maker.Make())
        return(false);

    return(true);
}
END_SHADER_NAMESPACE
