# ULRE.ShaderMaterialData —— 材质/着色器「数据面」。
#
# 这一层是运行时真正需要的：TOML 材质定义装载/材质配方解析、渲染模板解析、SPV 产物库、
# ShaderCreateInfo 的纯数据成员、采样器预设表。它零 GLSL 文本发射、零 glslang，
# 因此发行形态（ULRE_ENABLE_SHADER_PROGRAM_BUILD=0）也照常链接。
#
# GlslModule* 与 ShaderCodeResourceManifest 是材质/着色器元数据，归本层；
# 物理阶段源解析、options 生成及 SPIR-V 编译则由 ULRE.ShaderProgramBuild 消费。
# 跨界的唯一**构建行为**通道是 inc/hgl/mtl/ShaderProgramBuildBackend.h —— 这个接缝
# 头的接口吃本层的材质定义 / 构建请求，所以它连同槽位 .cpp 都归本层。
#
# 头清单归属判据：本层有对应 .cpp，或本层源文件直接包含该头。

set(SHADERMATERIALDATA_FILES
    # ── 材质定义与注册表 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialDefinitionFile.h
    MaterialDefinitionFile.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialDefinitionRegistry.h
    MaterialDefinitionRegistry.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialRecipeFile.h
    MaterialRecipeFile.cpp
    # 内嵌 builtin 材质定义（保底链的单一真源，不依赖资产目录）。
    ${ROOT_INCLUDE_PATH}/hgl/mtl/BuiltinMaterialDefinitions.h
    BuiltinMaterialDefinitions.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialRecipe.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialVertexVaryingConfig.h
    # ── GLSL code-module 描述、元数据与文件注册表（纯数据/资源闭包）──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/GlslModule.h
    GlslModule.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/GlslModuleFile.h
    GlslModuleFile.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/GlslModuleMetadata.h
    GlslModuleMetadata.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/GlslModuleRegistry.h
    GlslModuleRegistry.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCodeResourceManifest.h
    ShaderCodeResourceManifest.cpp
    # ── 表面领域契约（Fixed-Code Domain Shaders 的单一真源）──
    # 只做「声明 → 契约校验」这一件事：零 GLSL 发射、零着色器编译，
    # 因此发行形态也必须链接（与 MaterialDefinitionFile 同层）。
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialDomain.h
    MaterialDomain.cpp
    # 主着色器的唯一可变输入（generated/frag_options.glsl）。它**发射 GLSL 文本**，
    # 是本层「零 GLSL 文本发射」表述的唯一例外，但边界仍然干净：发射的是一份
    # 十几行的宏表（兼作 program 身份），不是可执行着色器源码 —— 本层依旧零
    # glslang、零模板拼装，发行形态（ULRE_ENABLE_SHADER_PROGRAM_BUILD=0）照常链接。
    ${ROOT_INCLUDE_PATH}/hgl/mtl/DomainOptions.h
    DomainOptionsEmitter.cpp
    # ── 领域主着色器模板表（领域 + pass → 固定 .frag 文件）──
    # 只有「查表」，没有拼装：废除生成器之后主着色器不再是生成物，
    # 运行时只需要知道该编哪个文件。发行形态照常链接。
    ${ROOT_INCLUDE_PATH}/hgl/mtl/DomainEntrySource.h
    DomainEntrySource.cpp
    # ── 渲染模板解析 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/PassProfile.h
    PassProfile.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ScenePassProfileResolver.h
    ScenePassProfileResolver.cpp
    # ── SPV 产物库 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderArtifactStore.h
    ShaderArtifactStore.cpp
    # ── ShaderCreateInfo 纯数据成员 + 构建上下文 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCreateInfo.h
    ShaderCreateInfo.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCreateInfoMap.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderBuildContext.h
    # ── 运行时 → 生成器接缝（槽位；两套实现先到先得）──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramBuildBackend.h
    ShaderProgramBuildBackend.cpp
    # ── 采样器预设 / 着色器库与缓存根路径 / 枚举文本 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/SamplerPreset.h
    SamplerPreset.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderLibraryPath.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCacheRoot.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/VkEnumText.h
    # ── 发行形态的两份查表清单 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramManifest.h
    ShaderProgramManifest.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramManifestKey.h
    ShaderProgramManifestKey.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramManifestBackend.h
    ShaderProgramManifestBackend.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderComputeProgramManifest.h
    ShaderComputeProgramManifest.cpp
    # ── 管线状态与顶点模式枚举（下层/运行时强依赖，程序构建侧仅作为上层消费者）──
    # 归属判据：MaterialRecipe.h / MaterialDefinitionFile.cpp 直接用它们，而
    # VKPipeline.h（Vulkan 层）持有 MaterialPipelineConfig 成员 —— 这些消费者都在
    # 本层或本层之下，程序构建侧只是上层的顺带使用者。
    ${ROOT_INCLUDE_PATH}/hgl/mtl/PipelineConfig.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MeshShaderMode.h
    ${ROOT_INCLUDE_PATH}/hgl/common/RenderTargetOutputConfig.h)

add_cm_library(ULRE.ShaderMaterialData "ULRE"
    ${ROOT_INCLUDE_PATH}/hgl/mtl

    ${SHADERMATERIALDATA_FILES}
)

# 数据面吃契约类型（ShaderProgramKey / ShaderArtifactContract / 设备 profile）。PUBLIC：
# 吃掉本库的目标同样需要这些类型。
target_link_libraries(ULRE.ShaderMaterialData PUBLIC ULRE.ShaderRuntimeContract
                                                    ULRE.Debug)
