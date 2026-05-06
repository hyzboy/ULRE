#include<hgl/shadergen/ShaderStageBuildSet.h>
#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl::graph::mtl
{
void ShaderStageBuildSet::DeleteAllShaders()
{
    if(!shader_map)
        return;

    for(auto [stage, sc] : *shader_map)
    {
        if(sc)
            delete sc;
    }

    shader_map->Clear();
}
}//namespace hgl::graph::mtl
