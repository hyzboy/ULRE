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

        // 材质源身份：决定用到哪些 .definition.toml / 模块源。
        h << definition.definition_id;

        // 着色器**变体**维度（不是配方实例维度）。
        //
        // 这里曾经折进 HashMaterialRecipe（整份实例身份：recipe_name、贴图
        // resource_id、SSBO 行绑定、纯 PSO 状态、回退标记色）。同一个 program_link
        // 因此会分裂成多条 lookup_key：烘焙期只走通其中一条，发行期用另一条查表
        // 必然 miss，现象是「发行版少了某一批材质」。变体键只收真正改变 GLSL 的
        // 字段，判据见 HashMaterialRecipeShaderVariant 的注释。
        const ResolvedMaterialRenderState resolved_state =
            ResolveMaterialRenderState(definition, request.recipe);
        h << HashMaterialRecipeShaderVariant(request.recipe, resolved_state);

        // 图元类型 + 顶点输入 + 编译目标档 + 用途 + ambient provider。
        // 编译目标档里含代码生成代数，跨代际的旧清单自然算出不同的键 ⇒ miss。
        h << HashMaterialProgramBuildContext(
                request.primitive_type,
                request.geometry_vertex_format,
                profile,
                request.shader_program_purpose,
                request.ambient_module_name);

        // 解析后的渲染模板请求：模板 ID / 版本 / 各槽位模块名与 include 路径。
        //
        // 这才是「实际用了哪份 fragment 源码」的权威描述。此前只靠
        // definition_id + ambient_module_name 间接推导，一旦调用方显式指定了
        // 模板（离线工具、回归夹具、probe）或环境层换了 ambient provider，
        // 两套不同源码就能算出同一个键 ⇒ 发行期取到不匹配的 SPV。
        // 必须吃**已解析**的请求（ResolveBuildRequest 的产物），不是调用方原始输入。
        h << request.render_template_request.GetHash();

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
