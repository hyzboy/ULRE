#include <hgl/mtl/ShaderProgramManifestKey.h>

#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    uint64 ComputeMaterialProgramManifestKey(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const contract::PhysicalDeviceProfileLite *profile) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        // 材质源身份：决定用到哪些 .material.toml / 模块源。
        h << definition.definition_id;

        // 配方全字段（含 render state / 纹理绑定 / SSBO 绑定）。
        h << HashMaterialRecipe(request.recipe);

        // 图元类型 + 顶点输入 + 编译目标档 + 用途 + ambient provider。
        // 编译目标档里含代码生成代数，跨代际的旧清单自然算出不同的键 ⇒ miss。
        h << HashMaterialProgramBuildContext(
                request.primitive_type,
                request.geometry_vertex_format,
                profile,
                request.shader_program_purpose,
                request.ambient_module_name);

        // 请求级顶点节点覆盖（配方里的那份已由 HashMaterialRecipe 覆盖）。
        if (request.has_vertex_node_config_override)
        {
            h << static_cast<uint32>(1)
              << request.vertex_node_config_override;
        }
        else
        {
            h << static_cast<uint32>(0);
        }

        return h;
    }
}//namespace hgl::graph::mtl
