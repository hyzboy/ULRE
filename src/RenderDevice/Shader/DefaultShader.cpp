#include<hgl/graph/shader/node/finished.h>

BEGIN_SHADER_NAMESPACE
bool CreateDefaultMaterial()
{
    node::Finished *vs_fin=new node::VertexFinished();
    node::Finished *fs_fin=new node::ForwardFinished();

    return(false);
}
END_SHADER_NAMESPACE
