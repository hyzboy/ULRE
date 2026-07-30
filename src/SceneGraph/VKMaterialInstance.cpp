#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialInstance.h>

namespace hgl::graph{
MaterialInstance *ShaderProgram::CreateMI(const VIL *vil)
{
    // In the new path ShaderProgram no longer owns a data store.
    // mi_id is always -1; data lives in an external SSBO managed by the caller.
    return new MaterialInstance(this, vil ? vil : GetDefaultVIL(), -1);
}

MaterialInstance *ShaderProgram::CreateMI(const VILConfig *vil_cfg)
{
    return CreateMI(CreateVIL(vil_cfg));
}

MaterialInstance *ShaderProgram::CreateMI(const GeometryVertexFormat &geometry_vertex_format)
{
    return CreateMI(CreateVIL(geometry_vertex_format));
}

MaterialInstance::MaterialInstance(ShaderProgram *mtl,const VIL *v,const int id)
{
    material=mtl;
    vil=v;
    mi_id=id;
}
}//namespace hgl::graph
