#pragma once

#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

// ═══════════════════════════════════════════════════════════════════════════════
// MaterialDomain — 表面领域契约（Fixed-Code Domain Shaders，阶段一）
// ═══════════════════════════════════════════════════════════════════════════════
//
// ## 为什么需要这一层
//
// 此前材质 TOML 可以任意组合「顶点语义集合 × 纹理槽集合 × 顶点提供策略」，
// ShaderGen 必须为每一种组合准备推导与兜底分支。组合是笛卡尔积，既无法穷举
// 测试，也让同一个渲染形态裂成大量互不兼容的顶点 ABI ⇒ 合批率被拖垮。
//
// 本层把「无限组合」收敛成**有限个具名领域**（Domain）。一个领域就是一份
// **不可协商的契约**：
//
//   · 需要哪些顶点语义（`requirements` 必须包含）
//   · 允许/必须声明哪些纹理槽（名字、语义角色、是否可缺省）
//   · 顶点提供策略（GeometryOnly / AllowDerived）
//
// 材质在 TOML 根层用 `domain = "StandardOpaque"` 声明自己属于哪个领域；
// 解析期即按契约校验，不满足直接失败（Fail-Fast，诊断报到表/键/值级别）。
// 未声明 `domain` 的材质等价于 `Custom`——不做任何契约校验，保持完全向后兼容。
//
// ## 与「光照/环境解耦」的关系
//
// 领域只描述**表面**：几何形态（顶点语义）、表面数据来源（纹理槽语义）。
// 领域**绝不**描述光照算法、GI、天光、阴影、Pass 输出格式——那些属于
// RenderPass / 环境层，通过 `ShaderLibrary/common/pass_environment.glsl`
// 的入口契约注入。因此同一个 `StandardOpaque` 表面既可用于 Forward Pass，
// 也可用于 GBuffer / Shadow(Depth-only) Pass，无需任何材质改动。
//
// ## 扩展方式
//
// 新增领域 = 在 `MaterialDomain` 枚举里加一项 + 在 MaterialDomain.cpp 的
// 契约表里加一行 + 提供 `ShaderLibrary/domain/<name>.frag` 固定模板。
// **不要**为了迁就某个资产而放宽已有领域的契约；那会让契约退化成建议，
// 本层也就失去意义。确实需要新形态时请新增领域。
// ═══════════════════════════════════════════════════════════════════════════════

namespace hgl::graph::mtl
{
    struct MaterialDefinition;

    // ── 领域枚举 ────────────────────────────────────────────────────────────────
    // Custom = 未声明领域（历史材质），不做契约校验。
    enum class MaterialDomain : uint8
    {
        Custom = 0,      // 无契约：保持 schema-4 的既有自由度
        StandardOpaque,  // 通用不透明/半透明硬表面（当前唯一已闭环领域）
        Foliage,         // 植被/树叶/草（风力摆动 + 双面 + 透射）
        Terrain,         // 多层混合地形
        UI2D,            // 界面/文本（无光照）
        HumanSkin,       // 人体皮肤（次表面散射）

        // ── 无光表面（业务可用）─────────────────────────────────────────────
        // 这一组与上面的区别：不做 PBR，表面值直接产出最终颜色。
        // 它们同样是**封闭契约**——不是「无光照的自由组合」。
        UnlitSolid,      // 纯色（材质行取色，无纹理）
        VertexColor,     // 顶点色（颜色直接来自 Color 顶点流）
        VertexPalette,   // 调色板顶点色（Color + TransformID + Size 驱动查表/变换）
        VertexLuminance, // 顶点亮度索引 → 调色板查表
        UnlitTexture,    // 无光单图（2D NDC 贴面）
        UnlitTexture3D,  // 无光单图（世界空间 3D 几何）
        Text2D,          // GPU 文本（SDF / 位图两态共用）

        // ── 环境与引擎内部用途 ──────────────────────────────────────────────
        // 下面这组由引擎代码按固定语义直接构造，不面向美术资产。
        // 它们仍被纳入领域体系，目的是让「Custom」不再是逃生门：
        // 任何材质都必须落在某个领域里，想新增组合就必须新增领域（有成本）。
        Sky,             // 天空（有/无 cube 贴图两态共用）
        DebugNormal,     // 调试法线可视化
        Checker2D,       // 调试棋盘格（2D）
        Checker3D,       // 调试棋盘格（3D）
        ErrorChecker3D,  // 材质错误回退棋盘格

        ENUM_CLASS_RANGE(Custom, ErrorChecker3D)
    };

    const char *GetMaterialDomainName(MaterialDomain domain) noexcept;

    // 非 Custom 的契约领域数量（用于契约表遍历与门禁穷举）。
    constexpr int MaterialDomainContractCount = 17;

    // 严格解析："StandardOpaque" 等；返回 false 表示未知名字（调用方报错）。
    bool ParseMaterialDomain(const char *name, MaterialDomain &out_domain) noexcept;

    // ── 纹理槽语义角色 ──────────────────────────────────────────────────────────
    // 角色是「这个槽在表面计算里代表什么」，不是「槽在哪个绑定位置」。
    // 绑定位置由 ShaderGen 按声明顺序排布，本层不参与。
    enum class MaterialDomainTextureRole : uint8
    {
        Unknown = 0,
        BaseColor,      // 基础色（乘性）
        Albedo,         // 反照率（替换）
        Normal,         // 切线空间法线
        Metallic,       // 金属度
        Roughness,      // 粗糙度
        Occlusion,      // 环境光遮蔽
        OpacityMask,    // 逐像素覆盖图案（与材质行 alpha 相乘）
        Transmission,   // 次表面透射（Foliage/Skin）
        SplatMap,       // 地形层权重
        Curvature,      // 曲率（Skin 预积分 SSS）
        Sprite,         // 2D 精灵/图集

        ENUM_CLASS_RANGE(Unknown, Sprite)
    };

    const char *GetMaterialDomainTextureRoleName(
        MaterialDomainTextureRole role) noexcept;

    // ── 单个纹理槽的契约条目 ────────────────────────────────────────────────────
    struct MaterialDomainTextureSlot
    {
        const char *name;                 // 材质 TOML 里必须使用的槽名
        MaterialDomainTextureRole role;   // 语义角色
        bool        must_declare;         // true = 材质必须声明此槽（可仍为 optional 绑定）

        // ── 规范纹理形态（阶段三之二：从资产上移到契约）─────────────────────────
        // 同一个领域内这三个值**必然相同**：它们描述的是「这个槽在领域里长什么样」，
        // 不是「这份资产想怎么用」。资产重复声明只会制造「同领域不同布局」，
        // 而布局差异会让 layout_hash 分叉、合批失效。
        //
        // 资产可以省略这三个键（解析层按契约兜底）；显式写了不同值 ⇒ 校验失败。
        SamplerType sampler;              // GLSL 采样器种类（唯一真源 TextureSamplerTypeDef.h）
        bool        binding_required;     // true = 运行时必须绑定（缺失 = 显式错误）
        uint32_t    channels;             // 0 = 由纹素格式决定；2 = 双通道法线(XY)
    };

    // ── 领域契约 ────────────────────────────────────────────────────────────────
    struct MaterialDomainContract
    {
        MaterialDomain domain;

        // 契约的可读名（= TOML 里 `domain = "..."` 的写法）。
        const char *name;

        // 该领域的**规范表面来源模块**与**规范法线标架模块**
        // （ShaderCodeModuleRegistry 里的模块名，不是文件路径）。
        //
        // 为什么用模块名而不是主着色器文件路径：顶点侧由 `[transform]`+
        // `[vertex]` 解析出 mesh options；片元侧由 DomainShaderTemplate 按领域与
        // pass 选择 ShaderLibrary/domain/ 下的固定 `.frag`。契约钉死的是
        // 「用哪个表面来源模块」，文件选路由独立的领域模板表完成。
        //
        // 门禁会核对这两个名字在模块注册表里确实存在，防止契约指向空气。
        // 领域尚未实现时两者置 nullptr（门禁跳过核对）——不要为凑数填一个
        // 「大致能用」的模块名，那会掩盖真实的缺失。
        const char *reference_material_source_module;  // 例：pbr_surface_source；nullptr = 未实现
        const char *reference_ntb_module;              // 例：ntb_tangent_vbo_normalmap；nullptr = 未实现

        // `[vertex] requirements` 必须包含的语义（bitmask，按 VertexSemantic 位）。
        uint32 required_vertex_semantic_mask;

        // 允许出现的纹理槽白名单；表外槽名 = 契约违规。
        const MaterialDomainTextureSlot *texture_slots;
        int texture_slot_count;

        // 顶点提供策略必须为 AllowDerived 时置 true（需要从几何流推导顶点）。
        bool require_allow_derived;

        // 该领域**结构上没有**表面来源模块（例：Sky 直接由方向矢量出颜色）。
        //
        // 为什么需要这个显式位：校验器对 reference_material_source_module == nullptr
        // 的领域默认要求资产自行声明来源模块（那是「领域未实现」的保守处理）。
        // 但 Sky 这类领域是「实现完成但不需要该模块」，两者不能共用 nullptr 表达，
        // 否则资产一行模块名都不写就会被判违规。填此位即豁免，且反过来禁止资产
        // 声明来源模块（声明了就是错配）。
        //
        // 注意：这个位不能用来给「未实现的领域」开后门偷懒。
        bool no_material_source;

        // 领域规范顶点变换（= 顶点通路 source/mapping/orientation/scale/projection）。
        //
        // 为什么放在领域契约里：`[transform]` 表达的是「这个领域怎么把顶点喂进管线」，
        // 不是美术的选择。全部内建材质统计下来只有三种形态（默认 3D / 2D NDC / 文字
        // Ortho），且与领域一一对应——资产重复写一遍只是在制造不一致的机会。
        //
        // 未显式填写的条目即取聚合初始化的默认值（默认 3D 配置），与历史行为一致。
        VertexShaderNodeConfig canonical_vertex_node_config;

        // 领域规范材质私有数据 SSBO 类型（`[resources].material_data.type`）。
        //
        // 为什么放在领域契约里：类型决定 shader 按哪个结构读 payload 行
        // （PBRSurface / EmissiveSurface / TransmissionSurface 字段宽度与顺序都不同），
        // 它不是美术参数。同一个领域混用两种类型 ⇒ 运行期按 A 结构读 B 结构的字节，
        // 不会报错，只会读到一堆看似合理的垃圾值。
        //
        // 必须显式填写（枚举 0 是 MeshDrawParams，不是材质表面类型）。
        GlobalSSBOType material_data_type;

        // 领域规范默认 Varying 语义掩码（双 Stage 接口契约）：
        // 若资产未手动声明 [vertex] varyings，解析层以此掩码为默认值自动推导。
        InterStageSemanticMask default_varying_semantic_mask = 0;
    };

    // 取契约；domain == Custom 或未知时返回 nullptr。
    const MaterialDomainContract *GetMaterialDomainContract(
        MaterialDomain domain) noexcept;

    // 按序号取契约表（0..MaterialDomainContractCount-1），供门禁穷举。
    const MaterialDomainContract *GetMaterialDomainContractAt(int index) noexcept;

    // 领域规范默认 Varying 掩码。Custom / 未知领域返回 0。
    InterStageSemanticMask GetMaterialDomainDefaultVaryingMask(
        MaterialDomain domain) noexcept;

    // 领域规范顶点变换。Custom / 未知领域返回 false（调用方保留资产声明或默认值）。
    bool GetMaterialDomainCanonicalVertexNodeConfig(
        MaterialDomain domain, VertexShaderNodeConfig &out) noexcept;

    // 按槽名取领域契约里的纹理槽条目。Custom / 未知领域 / 槽名不在白名单时返回 nullptr。
    const MaterialDomainTextureSlot *FindMaterialDomainTextureSlot(
        MaterialDomain domain, const char *slot_name) noexcept;

    // 领域规范材质私有数据 SSBO 类型。Custom / 未知领域返回 false。
    bool GetMaterialDomainCanonicalMaterialDataType(
        MaterialDomain domain, GlobalSSBOType &out) noexcept;

    // ── 领域规范模块（阶段四：资产只填领域 + 值 + 图）────────────────────────────
    // 材质 TOML 声明 `domain` 之后，**不必**再写 `material_source_module_name`
    // 与 `ntb_module_name`：解析层用这两个函数取回领域的规范模块名兜底填入。
    // 契约尚未实现（reference 为 nullptr）或 domain == Custom 时返回 nullptr。
    //
    // 注意取用时机：必须在校验**之后**。校验要看到作者的原始声明，才能判定
    // 「是不是在自选算法」（MaterialSourceModuleMismatch / NtbModuleMismatch）。
    const char *GetMaterialDomainReferenceMaterialSource(MaterialDomain domain) noexcept;
    const char *GetMaterialDomainReferenceNtb(MaterialDomain domain) noexcept;

    // 顶点语义 → 契约 bitmask 位。
    inline constexpr uint32 MaterialDomainSemanticBit(VertexSemantic semantic) noexcept
    {
        return 1u << static_cast<uint32>(semantic);
    }

    // 领域契约 → `[vertex] requirements` 需求列表（领域材质的单一真源）。
    //
    // 为什么可以推导：`required_vertex_semantic_mask` 就是「这个领域对几何的前提」，
    // 资产再抄一遍只给「抄漏」留机会——抄漏不会报错，运行期会拿缺属性的几何喂管线。
    // 顺序按 `VertexSemantic` 枚举升序，稳定可预期（消费方只按语义查表，不看顺序）。
    //
    // Custom / 未知领域返回 false：没有契约可依，必须由资产显式声明。
    bool GetMaterialDomainVertexSemanticRequirements(
        MaterialDomain domain,
        ValueArray<ShaderCodeModuleSemanticRequirement> &out) noexcept;

    // ── 校验结果 ────────────────────────────────────────────────────────────────
    enum class MaterialDomainValidationResult : uint8
    {
        OK = 0,
        UnknownDomain,              // domain 名不认识
        NoContract,                 // 非 Custom 但契约表里没有（内部错误，非「视为通过」）
        MissingVertexSemantic,      // requirements 缺少契约要求的顶点语义
        UndeclaredRequiredTexture,  // 契约要求声明的纹理槽在材质里缺失
        UnexpectedTextureSlot,      // 材质声明了契约白名单之外的纹理槽
        VertexProviderPolicyMismatch, // 顶点提供策略与契约不符
        MissingMaterialSource,      // 契约要求 material_source_module_name，但材质未声明
        MaterialSourceModuleMismatch, // 材质自选表面来源模块，与领域规范模块不符
        NtbModuleMismatch,          // 材质自选法线标架模块，与领域规范模块不符
        TransformMismatch,          // 材质自选顶点变换，与领域规范变换不符
        TextureSlotMismatch,        // 材质显式声明的纹理槽形态（采样器/绑定/通道数）与领域规范不符
        MaterialDataTypeMismatch,   // 材质显式声明的 material_data 类型与领域规范不符

        ENUM_CLASS_RANGE(OK, MaterialDataTypeMismatch)
    };

    const char *GetMaterialDomainValidationResultName(
        MaterialDomainValidationResult result) noexcept;

    // 契约校验。失败时 `out_detail` 给出「哪个槽/哪个语义 + 期望什么」，
    // 直接用于解析期诊断（与 MaterialDefinitionFileData::ParseErrorDetail 配套）。
    // Custom 领域恒返回 OK。
    MaterialDomainValidationResult ValidateMaterialDefinitionDomain(
        const MaterialDefinition &definition,
        MaterialDomain domain,
        AnsiString &out_detail) noexcept;
}// namespace hgl::graph::mtl
