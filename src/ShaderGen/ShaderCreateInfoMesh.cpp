#include<hgl/shadergen/ShaderCreateInfoMesh.h>

namespace hgl::graph{

ShaderCreateInfoMesh::ShaderCreateInfoMesh(MaterialDescriptorDB *m)
    :ShaderCreateInfo(new MeshShaderStageIO(),m)
{
    mesh_stage_io=static_cast<MeshShaderStageIO *>(stage_io);
}

}//namespace hgl::graph
