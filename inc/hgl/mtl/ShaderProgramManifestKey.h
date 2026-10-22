/**
 * ShaderProgramManifestKey.h —— 运行时可自算的「材质 program 语义键」。
 *
 * 这个键是发行形态查 `ShaderProgramManifest` 的唯一入口，因此它必须满足两条
 * 硬要求：
 *
 * 1. **纯数据**：只吃材质定义 id、配方、图元类型、顶点格式、编译目标档、用途、
 *    ambient provider 与顶点节点覆盖——一个字节的 GLSL、一次文件读取都不能有。
 *    这正是 `ULRE_ENABLE_SHADERGEN=0` 下运行时还能算出来的全部维度。
 *
 * 2. **完整决定 program key**：生成器读到的输入恰好就是这些（材质源与模块源由
 *    `definition_id` 决定，其余由配方/请求决定，磁盘上的 ShaderLibrary 在一份
 *    烘焙产物内固定）。少折任一维度，就可能取到不匹配的 SPV。
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
