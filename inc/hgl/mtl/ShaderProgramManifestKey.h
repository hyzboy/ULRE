/**
 * ShaderProgramManifestKey.h —— 运行时可自算的「材质 program 语义键」。
 *
 * 这个键是发行形态查 `ShaderProgramManifest` 的唯一入口，因此它必须满足三条
 * 硬要求：
 *
 * 1. **纯数据**：只吃材质定义 id、配方**变体**维度、图元类型、顶点格式、编译
 *    目标档、用途、ambient provider、已解析的渲染模板请求与顶点节点覆盖——一个
 *    字节的 GLSL、一次文件读取都不能有。
 *    这正是 `ULRE_ENABLE_SHADERGEN=0` 下运行时还能算出来的全部维度。
 *
 * 2. **完整决定 program key**：生成器读到的输入恰好就是这些（材质源与模块源由
 *    `definition_id` 与渲染模板请求决定，其余由配方/请求决定，磁盘上的
 *    ShaderLibrary 在一份烘焙产物内固定）。少折任一维度，就可能取到不匹配的 SPV。
 *
 * 3. **只含变体维度，不含实例维度**：配方按 `HashMaterialRecipeShaderVariant`
 *    折叠（alpha_test/alpha_cutoff/dither/alpha_blend/vertex_node_config），
 *    **不**折 recipe_name、贴图 resource_id、SSBO 行绑定、纯 PSO 状态与回退标记色。
 *    把这些折进来会让同一条 program_link 在清单里分裂成多条键，烘焙期只填一条，
 *    发行期用另一条查表即 miss。
 *
 * 刻意**不**折进设备的 mesh invocation 上限：它对同一个 tier 是可变上限，
 * 折进去等于按机型分裂 SPV，发行版又回到「一套机型一份产物」。正确做法是
 * 烘焙时按目标 tier 的**下限**烧制（见 `ClampMeshInvocationsByDevice`）。
 */
#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>

namespace hgl::graph::mtl
{
    uint64 ComputeMaterialProgramManifestKey(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const contract::PhysicalDeviceProfileLite *profile) noexcept;
}//namespace hgl::graph::mtl
