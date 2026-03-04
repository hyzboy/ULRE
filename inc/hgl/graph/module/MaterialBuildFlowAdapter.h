#pragma once

#include <hgl/type/String.h>

namespace hgl::graph
{
    class MaterialManager;
    class ShaderCreateInfoMap;
    class ShaderModuleMap;
    class VertexInput;
    class MaterialDescriptorManager;

    namespace mtl
    {
        class MaterialCreateInfo;

        namespace contract
        {
            struct ShaderGenResult;
        }
    }

    bool BuildShaderModulesFlow(MaterialManager *manager,
                                const AnsiString &mtl_name,
                                const ShaderCreateInfoMap &sci_map,
                                const mtl::contract::ShaderGenResult *mirror_result,
                                bool require_mirror_valid,
                                ShaderModuleMap *shader_maps,
                                bool &mirror_spv_build_used);

    bool BuildMaterialBindingsFlow(const AnsiString &mtl_name,
                                   const mtl::MaterialCreateInfo *mci,
                                   const mtl::contract::ShaderGenResult *mirror_result,
                                   bool mirror_spv_build_used,
                                   bool require_mirror_valid,
                                   VertexInput *&out_vertex_input,
                                   MaterialDescriptorManager *&out_desc_manager);
}
