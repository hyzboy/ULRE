#pragma once

namespace hgl::graph::mtl {}

#include <hgl/type/DataType.h>
#include <hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl::contract
{
    using namespace hgl::graph::mtl;

    // 硬性 Vulkan 1.4（vulkan1.4.md 第 1 项）：版本/SPIR-V 阶梯整体塌缩为常量。
    // 1.3 起 SPIR-V 1.6 即核心，1.4 下无条件成立——不再按 api_version 探测。
    constexpr uint32_t MakeVkVersion(const uint32_t major, const uint32_t minor, const uint32_t patch = 0)
    {
        return (major << 22) | (minor << 12) | patch;
    }

    constexpr uint32_t VkVersionMajor(const uint32_t version)
    {
        return version >> 22;
    }

    constexpr uint32_t VkVersionMinor(const uint32_t version)
    {
        return (version >> 12) & 0x3ffu;
    }

    constexpr uint32_t SPV_VERSION_1_6 = (1u << 16) | (6u << 8);
    constexpr uint32_t TARGET_VULKAN_VERSION = MakeVkVersion(1, 4);
    constexpr uint32_t TARGET_SPV_VERSION = SPV_VERSION_1_6;

    // 代码生成代数：GLSL 发射逻辑语义变化（如材质数据 Arena+BDA 改造）时 +1，
    // 一次性整体更换全部 SPV artifact 缓存键（防新旧生成器产物混用）
    //
    // 5：stage key 的 definition_hash / interface_hash 从「已生成的 GLSL 文本」
    //    换成「emit 之前的声明源」。同一份输入算出的键值必然与旧版不同，
    //    旧 SPV 缓存全部作废——正是这里换代的目的。
    // 6：interface_hash 折叠 inter-stage 条目时不再吃结构体 padding（改走
    //    InterStageSemanticContractEntry 的显式逐字段 operator<<），键值随之变化。
    // 7：Sky 模板整删纯校验用的 OutputPolicy slot（sky.glsl.tmpl 没有对应占位符，
    //    该模块从不发射）。resolved_template_hash 随 module_root_count 变化，
    //    sky 程序缓存自然失效；这里一并换代，避免新旧声明闭包混用。
    // 8：编译目标哈希不再折叠物理设备身份（vendor_id / device_id / api_version /
    //    limits / features）。SPIR-V 是平台中立字节码，这些字段只影响 glslang 的
    //    校验门槛，不改变发射出的指令；唯一真正进 GLSL 文本的设备量是 mesh
    //    invocation 上限，它早已由 ClampMeshInvocationsByDevice 折进
    //    definition_hash / interface_hash。此前把它们塞进 compiler_hash，等于同一份
    //    SPV 按显卡型号各存一份 —— 发行版无法「只打包一套 SPV」。键值整体变化，
    //    这里换代作废旧缓存。
    // 9：法线贴图 provider 从「世界轴任意正交基」改为 dFdx/dFdy 余切标架（#12），
    //    并删掉了与之重复的 ntb_derivative_normalmap。模块图与模块文本都变了，
    //    虽然 definition_hash 本来就会跟着变，但这里换代是为了把 Stage 1~4 期间
    //    累积的旧产物一次性清空，避免新旧声明闭包混用。
    // 10：烘焙清单 entry 的 flags 新增 bit1（requires_recipe_payload_row）——运行期
    //     材质私有数据 SSBO 绑定校验改读该位，不再拿合并信号
    //     `RequiresRecipeRuntimeRows()` 当 payload 需求。清单 payload 里这一位的
    //     语义变了（旧产物恒 0），必须换代，否则旧清单会让纯纹理材质再次被误杀。
    // 11：payload 行需求的判据从「有纹理声明」改为「材质提供的 GLSL 模块声明了
    //     `require Resource MaterialData`」（material_source 与 surface 两个槽位）。
    //     无纹理材质（VertexColor / VertexPaletteColor / Checkerboard2D/3D /
    //     纯 Surface 天空）不再需要 material_ssbo_binding，bit1 的取值随之改变；
    //     第 10 代产物里这些材质的 bit1 可能是错的，必须换代重烘。
    // 12：新增回退（错误）材质 `builtin/error_checkerboard_3d` + 其 fragment 模块
    //     `error_checkerboard_3d_source`（读 EmissiveSurface 标记色作棋盘格两色基色），
    //     回退规则表的两个错误种类从 `builtin/checkerboard_3d` 改指到它 ⇒ 该材质的
    //     bit1 从 0 变 1，并且材质库多出一份定义。第 11 代产物里没有这份材质，
    //     必须换代重烘。
    // 13：新增「顶点缺法线 ⇒ 面法线光照」降级发射维度 —— 顶点格式没有 Normal 语义
    //     而材质要法线光照时，剥掉 Normal/Tangent/Bitangent 需求并发射
    //     `HGL_MATERIAL_FACE_NORMAL_FALLBACK` define，fragment 端用世界位置的屏幕
    //     空间导数叉积求三角形面法线。这一维直接改变 definition_hash 与发射出的
    //     GLSL 文本；第 12 代产物里 `Lit` 等材质在同样输入下走的是顶点法线路径
    //     （读未上传的 Normal 槽），必须换代重烘。
    // 14：ShadowCaster 的两个模板从 C++ 手写块发射改为外置骨架
    //     `fragment/shadow_caster.glsl.tmpl`（Stage 6，后续已由固定领域 `.frag` 取代）。
    //     骨架是单 MainBody 块，
    //     与 forward 模板一致，于是 `HGL_COVERAGE_ONLY` / `HGL_ALPHA_*` 的发射位置
    //     从「材质注入前置块之前」移到「之后」——宏的集合与文本不变，但**字节顺序
    //     变了**，深度/阴影程序必须换代重烘。golden `lit-*-masked.emission.txt` /
    //     `lit-*-depth.emission.txt` 同步刷新。
    // 15：均匀化「program / stage 键的形状」——
    //      · `ShaderStageKey` 5→4：`glsl_module_graph_hash` + `resource_hash` 合成
    //        `compile_inputs_hash`（options 摘要不再靠 `// domain-options-key`
    //        注释混进 definition_hash，改为显式进 compile_inputs_hash）；
    //      · `ShaderProgramKey` / `ShaderLinkSpec` 的 4 个 link 哈希 → 1 个
    //        `link_contract_hash`；
    //      · `ShaderProgramArtifactMetadata` 9→2、两份 manifest 升 v2。
    //      GLSL 文本与 SPV 字节不变，但**所有键值、缓存文件名、清单格式全变**，
    //      旧产物既不会被命中也不会被读出，必须换代重烘。
    constexpr uint32_t SHADERGEN_CODEGEN_GENERATION = 15;

    // 物理设备量（vendor/device/api_version/limits/features）**不得**折进任何
    // SPV 缓存键：SPIR-V 是平台中立字节码，设备量只影响 glslang 的校验门槛，
    // 不改变发射出的指令；唯一真正进 GLSL 文本的设备量是 mesh invocation 上限，
    // 它由 ClampMeshInvocationsByDevice 折进 definition_hash / interface_hash。
    // 设备量一旦进键，一个 SPV 就得按显卡型号各存一份，发行版无法「只打包一套」。
    //
    // 缓存键统一用 GetShaderCompilerProfileHash（见下）。

    // 编译目标哈希 —— **全部** SPV 缓存键（stage key / program key / artifact
    // metadata）唯一允许使用的编译器身份维度。
    //
    // 只吃三样常量：Vulkan 目标版本、SPIR-V 目标版本、代码生成代数。这三者才是
    // 真正决定 SPV 字节内容的因素；物理设备量一个都不进（理由见上一段）。
    //
    // `profile` 参数保留，一是守住调用点形状，二是未来若真出现「某设备必须换
    // 产物」的需求时有落点；当前实现刻意不读它。
    inline uint64 GetShaderCompilerProfileHash(
        const PhysicalDeviceProfileLite *profile) noexcept
    {
        (void)profile;

        uint32 vulkan_version = TARGET_VULKAN_VERSION;
        uint32 spv_version = TARGET_SPV_VERSION;

        hgl::hash::FNV1aHasher64 h;

        h << vulkan_version
          << spv_version
          << SHADERGEN_CODEGEN_GENERATION;

        return h;
    }
}
