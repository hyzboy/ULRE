#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>

namespace hgl::graph
{
    class ShaderCreateInfoMesh:public ShaderCreateInfo
    {
        MeshShaderStageIO *mesh_stage_io;

    public:

        MeshShaderStageIO *GetMeshStageIO(){return mesh_stage_io;}
        const MeshShaderStageIO *GetMeshStageIO()const{return mesh_stage_io;}

    public:

        ShaderCreateInfoMesh(MaterialDescriptorDB *m);
        ~ShaderCreateInfoMesh()override=default;
    };//class ShaderCreateInfoMesh
}//namespace hgl::graph
