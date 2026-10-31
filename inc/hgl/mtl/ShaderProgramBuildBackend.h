/**
 * ShaderProgramBuildBackend.h —— 运行时 → 着色器构建期的**唯一接缝**。
 *
 * 背景（ShaderGen 物理切割）：
 *   `ShaderProgramManager::AcquireShaderProgram()` 需要两件构建期才能做的事：
 *     · `CreateBuildContext`        —— 建 ShaderBuildContext（网格阶段 GLSL + 编译规格）
 *     · `FinalizeShaderBuildContext` —— glslang 编译 / SPV 缓存读写
 *   发行版不带构建期，这两件事必须由**别的东西**顶上（预烘焙 SPV Manifest）。
 *
 * 所以运行时只认这个抽象接口，不认识具体构建器：
 *   · 开发形态（`ULRE_ENABLE_SHADER_PROGRAM_BUILD=1`）：`InitShaderCompiler()` 时装入
 *     领域后端（`DomainShaderBuildBackend.cpp`）——片段阶段是物理固定的领域主
 *     着色器，网格阶段按需发射。
 *   · 发行形态（`ULRE_ENABLE_SHADER_PROGRAM_BUILD=0`）：`EnsureShaderProgramManifestBackend()`
 *     装入 `ShaderProgramManifestBackend`，全部 SPV 来自预烘焙清单，不碰 GLSL。
 *   · 两者都没装 ⇒ 取到 nullptr，`ShaderProgramManager` 明确报错。
 *   两套实现**先到先得**，后装的不覆盖先装的。
 *
 * 归属（为什么在数据面而不是契约层）：
 *   接口签名吃 `MaterialDefinitionBuildRequest` / `MaterialDefinition` / `ShaderBuildContext`，
 *   这些类型都属于本层（ULRE.ShaderMaterialData）。放在契约层会逼着 Contract 反向依赖
 *   材质定义与构建期请求，与「契约层零运行时依赖」相冲突。
 *
 * 硬约束：
 *   · 本头只描述接缝，不含 GLSL / 不含编译器插件 / 不含 IO；
 *   · 实现方（领域后端或 Manifest）自己持有全部构建期状态；
 *   · 槽位是**进程级单例指针**，装载由 `InitShaderCompiler()` / Manifest 装载器负责，
 *     调用方不得自己收藏指针后又假定它长期有效。
 */
#pragma once

#include <hgl/mtl/MaterialDefinitionRegistry.h>

namespace hgl::graph::mtl
{
    class ShaderBuildContext;

    struct ShaderProgramBuildBackend
    {
        virtual ~ShaderProgramBuildBackend() = default;

        /** 全量构建：发射 GLSL、建立 program link / artifact metadata，产出可用的构建上下文。 */
        virtual ShaderBuildContext *CreateBuildContext(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request) = 0;

        /** 收尾：把构建上下文里的每个阶段变成真正可用的 SPV（编译或读缓存）。 */
        virtual bool FinalizeBuildContext(ShaderBuildContext *ctx) = 0;
    };

    /** 装入/清除后端（传 nullptr 清除）。非线程安全，只在初始化/关闭期调用。 */
    void SetShaderProgramBuildBackend(ShaderProgramBuildBackend *backend);

    /** 当前后端；nullptr = 本形态下没有可用的构建后端。 */
    ShaderProgramBuildBackend *GetShaderProgramBuildBackend();
}//namespace hgl::graph::mtl
