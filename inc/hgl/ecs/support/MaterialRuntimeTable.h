/**
 * MaterialRuntimeTable.h —— **材质运行期层**（v2 §9.3 A4）
 *
 * 用途：把已删除的**材质运行期组件**里**随授权态变化、可跨实体共享**的绑定状态
 *       收敛成「世界私有的**共享行**存储」+「每实例小记录（slot）」两层：
 *
 *         · **共享行（MaterialRuntimeRow）**：interned —— 同键的多实体引用同一行
 *           （变体 ID / 材质 SSBO 行与地址 / 纹理引用行与 hash / 已解析配方缓存）。
 *           行内**不得**出现 per-instance / per-frame 语义的字段（当前 pass、LOD 档、
 *           是否 dither、重试/降频计数、脏标志……）——那些一律归 slot 侧。
 *         · **写时分裂（CoW，`DetachOwned`）**：只有**持久差异**（改了参数 / 纹理覆盖 /
 *           自有 SSBO 行）才让实例脱离共享行。差异通常表现为**键变了**（改参数或纹理
 *           覆盖 ⇒ `recipe_hash` 变；自有 SSBO 行 ⇒ `data_index_row` 变）⇒ 下次
 *           `Intern` 自然落到新行；`DetachOwned` 覆盖"同一实例必须就地独立、且
 *           键还没变"的显式分裂路径（分裂出的**自有行**不入键索引）。
 *
 *           ⚠ **现状（2026-10-10 审计 §1.1 复核）：生产路径当前不调用 `DetachOwned`** ——
 *           所有差异都压进了键（见下「键的规矩」），且对共享行的写入被约束为**键的纯函数**
 *           （幂等：见 `RenderPrimitiveCollectSystem` 里刷新 `cached_effective_recipe*` 处的注释）
 *           ⇒ 共享在语义上安全、无需分裂。它**保留**为该模型的第二条路径（安全阀），
 *           **使用条件** = 要写一个「不入键、且只属于该实例」的行内绑定字段时（直接 `GetMutable`
 *           写共享行会让同键的其它实例一起变 = 串味）；也可用于「先分裂、后续帧原地改」的
 *           编辑/调试流（分裂是内容拷贝 ⇒ 绑定地址不变、不重算；自有行不入键索引 ⇒ 键空间不随
 *           实体数增长）。**别把它当死代码删**：`owned` 已织进表的账目 —— `RecycleRow` 据它
 *           决定是否摘键索引，`GetSharedRowCount` / `GetOwnedRowCount` 也都读它。
 *         · **refcount**：`Intern` / `AddRef` 加引用，`Release` 减引用，**归零回收**
 *           （回收时经世界退休该行的纹理配置池行 —— GPU 侧绑定随行存亡）。
 *         · **世代**：行号复用按 **`+2` 保持奇数**（与 `TransformDataStorage` /
 *           `BoundingBoxDataStorage` 同约定）：`0 = 死/未分配，正奇数 = 活`，
 *           ABA 免疫（`GetGeneration` 对死行恒返回 0）。
 *
 * 键的规矩（**只放"随授权态"的维度**；判据 = 这一维变没变"该行承载的绑定"）：
 *   · `program_build_context` —— program 身份（图元类型 + 顶点格式 + 设备 profile + purpose）；
 *   · `recipe_hash`           —— 材质配方身份（`graph::mtl::HashMaterialRecipe`）；
 *   · `data_index_row`        —— 材质 SSBO 行号（**自有 SSBO 行**的判据）。
 *     ⚠ **必须入键**：`HashMaterialRecipe` **不含** `material_ssbo_binding.data_index`
 *     （见 `inc/hgl/mtl/MaterialRecipe.h:641-648`，只哈希 ssbo_type/ssbo_id）——只按
 *     recipe 哈希去重会把"同配方、不同数据行"的实体并成一行（PBRSpheres 每个球一行
 *     数据 ⇒ 全体材质串味）。
 *   · **绝不能入键**：每实例选择器（pass / LOD / dither）、帧号、脏标志、重试计数、
 *     行地址与纹理配置 —— 它们是**行的产物**，不是行的身份（放进去会让表随帧/实体
 *     无界增长，正是 v2 §9.3 要避免的形态）。
 *   · **键必须静态且有界**：三个维度都只由"材质授权态 + 图元/设备静态属性"决定。
 *
 * 归属：表由 `ECSContext` 持有（与 `VisibilityDataStorage` / `MaterialVariantTable` 同范式，
 *       见 `Context.h::GetMaterialRuntimeTable()`）——行里的绑定指向本世界 GraphicsContext
 *       的资源，表随世界存亡，不会跨 GraphicsContext 混用。
 *
 * 预算制：容量上限 `kMaterialRuntimeCapacityLimit`，超限 **fail-fast**（`Intern` 返回
 *       `INVALID_MATERIAL_RUNTIME_ROW_ID`）并**只告警一次**——与 `MaterialVariantTable`
 *       的 4096 同口径，不静默扩容。
 */
#pragma once

#include<hgl/ecs/support/MaterialVariantTable.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/graph/module/MaterialTextureReferencePool.h>
#include<hgl/graph/render/RenderItemDescriptor.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/type/UnorderedMap.h>
#include<cstdint>
#include<cstddef>
#include<type_traits>
#include<vector>
#include<unordered_map>

namespace hgl
{
    namespace graph
    {
        class RenderPass;
        class Pipeline;
        class DeviceBuffer;
        class IndirectMeshTaskBuffer;
    }
}

namespace hgl::ecs
{
    class ECSContext;

    /// 共享行句柄：行下标；`INVALID_MATERIAL_RUNTIME_ROW_ID` = 未登记/已回收。
    using MaterialRuntimeRowID = uint32_t;

    inline constexpr MaterialRuntimeRowID INVALID_MATERIAL_RUNTIME_ROW_ID =
        static_cast<MaterialRuntimeRowID>(-1);

    /// 共享行键：**只由授权态决定**（见文件头"键的规矩"）。同一份授权态 ⇒ 同一个键
    /// ⇒ 同一行（interned）；只要有一维不同就必然分裂成两行（不会串味）。
    struct MaterialRuntimeKey
    {
        /// program 身份（build context 哈希）
        uint64_t program_build_context = 0;

        /// 材质配方身份（effective recipe 的 `HashMaterialRecipe`）
        uint64_t recipe_hash = 0;

        /// 材质 SSBO 行号（授权态的"自有 SSBO 行"维度；无绑定记 0）
        uint32_t data_index_row = 0;

        bool operator==(const MaterialRuntimeKey &rhs) const noexcept
        {
            return program_build_context == rhs.program_build_context
                && recipe_hash == rhs.recipe_hash
                && data_index_row == rhs.data_index_row;
        }

        bool operator!=(const MaterialRuntimeKey &rhs) const noexcept
        {
            return !(*this == rhs);
        }
    };


    // ── 键空间权威链（2026-10-13 审计 §2.2 问题 5 选项 (b)）────────────────────────────
    //   同一实体的"材质态"被四条键描述，它们是**同一件事的四个投影**，各自服务不同消费者
    //   （**不该合并**：合并会让共享退化成每实例/每行一份，见审计报告 07 §三）：
    //
    //   | 键 | 维度 | 权威/产生步 | 消费者 |
    //   |---|---|---|---|
    //   | ① `MaterialVariantKey`（本报告的前缀） | build_context + recipe_hash | collect 解析 program 时 | program 记录（变体表；program 本体归 ShaderProgramManager） |
    //   | ② `MaterialRuntimeKey`（= ① + `data_index_row`） | ① + 自有 SSBO 行 | collect 物化材质行之后（`Intern`） | 共享行（绑定状态：变体 ID / SSBO 行与地址 / 纹理配置 / 配方缓存） |
    //   | ③ `MaterialAuthoringSideTable`（**行 id 空间**） | 材质 SSBO 行号 | 作者流程写行 | `data_index_row` 的来源（② 的第 3 维与它同源） |
    //   | ④ `MaterialRuntimePipelineTable`（**每实例每 pass**） | EntityID + pass | 绘制前解析管线 | 复用已解析管线（**不是身份**，不该进 ①②） |
    //
    //   先后：③（授权行）⇒ ①（program 身份）⇒ ②（共享行身份，含 ③ 的行号）⇒ ④（每实例解析产物）。
    //   ⚠ **只有 forward 变体键是运行期键的前缀**：阴影变体键用 **ShadowDepth** 的 build context
    //     （与 forward 行的键同 recipe、不同 purpose）⇒ 它是**兄弟键**，不是前缀（见
    //     `RenderPrimitiveCollectSystem` 阴影侧的注释）。

    /// 变体键 = 运行期键去掉「授权 SSBO 行」维 —— **前缀关系的唯一实现**（放本文件是因为
    /// 只有这里能同时看到两个键；`MaterialVariantKey` 的定义在
    /// `MaterialVariantTable.h`，其哈希经 `HashMaterialProgramIdentity()` 共用同一份折字段）。
    /// 规矩：加「配方身份」维度**先改 `MaterialRuntimeKey`**，本函数自动跟随；不决定 program
    /// 身份的维度不得进前缀。生产接线见 `RenderPrimitiveCollectSystem` 的 forward 路径
    /// （变体键由本函数从运行期键派生 ⇒ 前缀关系**由构造保证**，不靠约定）。
    inline MaterialVariantKey VariantKeyOf(const MaterialRuntimeKey &k) noexcept
    {
        return MaterialVariantKey{ k.program_build_context, k.recipe_hash };
    }

    // 前缀的**编译期**钉死：类型 / 偏移逐字相同、第 3 维排在前缀之后。
    // ⇒ "改一处漏一处"（只给其中一个键加维）会在这里立刻编译失败，而不是运行期静默分叉。
    static_assert(std::is_same_v<decltype(MaterialVariantKey::program_build_context),
                                 decltype(MaterialRuntimeKey::program_build_context)>,
                  "前缀第 1 维类型必须与 MaterialRuntimeKey 一致（改维度请改 MaterialRuntimeKey，见 VariantKeyOf 规矩）");
    static_assert(std::is_same_v<decltype(MaterialVariantKey::recipe_hash),
                                 decltype(MaterialRuntimeKey::recipe_hash)>,
                  "前缀第 2 维类型必须与 MaterialRuntimeKey 一致（改维度请改 MaterialRuntimeKey）");
    static_assert(offsetof(MaterialRuntimeKey, program_build_context)
                == offsetof(MaterialVariantKey, program_build_context),
                  "前缀第 1 维偏移必须与 MaterialRuntimeKey 一致（逐字相同的前缀）");
    static_assert(offsetof(MaterialRuntimeKey, recipe_hash) == offsetof(MaterialVariantKey, recipe_hash),
                  "前缀第 2 维偏移必须与 MaterialRuntimeKey 一致（逐字相同的前缀）");
    static_assert(offsetof(MaterialRuntimeKey, data_index_row) >= sizeof(MaterialVariantKey),
                  "第 3 维必须排在前缀之后（它是唯一的后缀维；插到前面就不成前缀了）");

    /// 键哈希（**仅**用于内部索引；相等性仍是全维度 `operator==`）。
    /// ⚠ 折字段必须走共用前缀 `HashMaterialProgramIdentity()` + 以它为种子续算第 3 维
    ///   （**禁**在此再手写一遍「前缀两维的左移折字段」：那正是「前缀两份实现」的形态；
    ///   本条同时被源码契约 Test 50 的禁复活针盯着）。
    struct MaterialRuntimeKeyHash
    {
        std::size_t operator()(const MaterialRuntimeKey &key) const noexcept
        {
            hgl::hash::FNV1aHasher64 h(
                HashMaterialProgramIdentity(key.program_build_context, key.recipe_hash));

            h << key.data_index_row;
            return static_cast<std::size_t>(h.Result());
        }
    };

    /// **共享行**：同键的多实体引用同一行（interned）。
    ///
    /// 字段准入判据（逐条见文件头）：**只放随授权态变化的绑定**。
    ///   · 变体 ID（program 身份所在；program 本体归 ShaderProgramManager 缓存持有）；
    ///   · 材质 SSBO 行号与 CPU/GPU 地址（由授权态 + 世界缓冲布局决定）；
    ///   · 纹理引用配置行（池行所有权随行：行归零回收时退休）；
    ///   · 已解析配方缓存（normalized / effective）与各自哈希。
    /// **行管理字段**（refcount / generation / owned / alive）不属于绑定内容。
    struct MaterialRuntimeRow
    {
        MaterialRuntimeKey key{};

        // ── 变体 ID（前向 / 阴影两个程序槽；`MaterialVariantTable.h`）──
        MaterialVariantID forward_variant = INVALID_MATERIAL_VARIANT_ID;
        MaterialVariantID shadow_variant  = INVALID_MATERIAL_VARIANT_ID;

        // ── 材质 SSBO 行（授权态：`data_index_row` 同时是行键的一维）──
        // 初值 `uint32_t(-1)` = **尚未物化**（批处理侧据此判定"行未就绪"）；
        // 物化完成写回真实行号（无绑定 ⇒ 0）。
        uint32_t data_index_row = uint32_t(-1);
        uint64_t material_row_gpu = 0;

        // ── 纹理引用配置（MaterialTextureReferencePool 的按 definition 池行）──
        graph::MaterialTextureConfigurationAllocation
                material_texture_configuration;
        uint64_t material_texture_row_gpu = 0;
        uint64_t material_texture_zero_row_gpu = 0;
        uint64_t material_texture_configuration_hash = 0;

        // ── 已解析配方缓存（同键实体共享同一份；program 解析的产物）──
        uint64_t recipe_hash = 0;
        graph::mtl::MaterialRecipe cached_normalized_recipe{};
        graph::mtl::MaterialRecipe cached_effective_recipe{};
        uint64_t cached_effective_recipe_hash = 0;
        graph::mtl::MaterialRecipe shadow_cached_normalized_recipe{};

        // ── 行管理（不是绑定内容；探针/测试用）──
        uint32_t refcount = 0;
        uint32_t generation = 0;    ///< 0 = 死/未分配；正奇数 = 活；复用时 +2
        bool     owned = false;     ///< true = CoW 分裂出的自有行（不入键索引）
        bool     alive = false;
    };

    /// **每实例小记录**：承载**每帧 / 每实例可变**的状态（绝不进共享行）。
    /// 按实体稀疏存放（世界持有；`GetOrCreateSlot`）。
    struct MaterialRuntimeSlot
    {
        /// 本实例引用的共享行（refcount 由本 slot 持有；释放见 `DestroySlot`）
        MaterialRuntimeRowID row = INVALID_MATERIAL_RUNTIME_ROW_ID;

        // ── 每实例选择器（本步只需能承载；stage B 由几何/材质实例数据填充）──
        uint8_t pass_selector = 0;      ///< 当前 pass（0 = forward，1 = shadow，…）
        uint8_t lod_selector  = 0;      ///< LOD 档
        bool    dither_enabled = false; ///< 是否 dither

        // ── D9：阴影 pass 跳过路径的重试/降频计数（**A4 从共享变体记录迁来**）──
        // 语义不变：该实例连续跳过帧数；首次跳过告警一次，连续超
        // kShadowRetryFullBumpFrames 后把静态级联失效降频为周期性；成功产出
        // render item 时复位。放每实例侧 ⇒ 同键健康兄弟不再清零失败者的计数。
        uint32_t shadow_retry_frames = 0;

        // ── 每实体授权代跟踪副本（与各自 MaterialData 的 GetAuthoredGeneration 比对）──
        uint32_t tracked_material_data_generation = 0;
        uint32_t shadow_tracked_material_data_generation = 0;

        // ── 生命周期 / 脏标志（每实例）──
        bool     program_dirty = true;  ///< program（管线）需重新解析
        bool     runtime_dirty = true;  ///< 绑定/资源需重新准备与物化
        bool     valid = false;         ///< 整条链（解析+准备+物化+几何+管线）成功过
        uint64_t last_materialize_epoch = 0;    ///< 上次物化所在的物化世代

        // ── A5b：渲染侧每实例绑定（原图元渲染组件的残余状态）──
        // 判据：与授权态无关、每实体一份的可变绑定 —— 一律归 slot，绝不进共享行（A4 约定）。

        /// render_item 4-ID 槽位句柄（世界 RenderItemDataStorage；按需分配，实体销毁时释放）
        graph::RenderItemHandle render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;

        /// 最近一次 collect 解析出的运行期管线（按 RenderPass 维度；batch 紧随其后消费同一 pass）。
        /// 复用交给 RenderPass::CreatePipeline 内部（按 shader 内容 hash 键控，见 D2）——
        /// **B5-b 起不入 slot**（行内不放指针）：落世界级 `MaterialRuntimePipelineTable`
        ///（键 = 本实体；读时比对 pass，语义与旧字段逐字一致）。

    };

    /// **行宽定稿**（B5-b）：`MaterialRuntimeSlot` 全字段均为 POD、**无任何指针** ⇒
    /// 宽度是平台稳定的定值，这就是它将来升格为 (World, MaterialRuntime) 行 arena 一行的前提
    ///（行宽一变，所有按行索引的并行表/地址都要跟着动 ⇒ 用断言钉死，改动必须显式）。
    /// 字段与占位：row 4 | 2×u8 + bool +pad 4 | shadow_retry 4 | 2×tracked 4 | 3×bool +pad 4 |
    /// last_materialize_epoch 8 | render_item_handle 4 ⇒ **x64/MSVC 实测 40 B**（探针 `ProbeTransformDiagnostics` 同源打印）。
    /// B5-a 的 6 个缓冲区句柄与世界级 `MaterialRuntimeBufferTable` 已整体退役
    ///（外部 GPU-Driven 入口系整体删除，见计划档）。
    /// ④ 组件多实例能力（实例三字段 + 8 个自由函数 + `InstancedPrimitiveRenderItem`）
    /// **won't-do 整体删除**：唯一活调用方是为「被拒」而存在的 A3 台架，无真实使用者
    ///（见计划档 §8.7 A2 关闭记录）。
    static_assert(sizeof(MaterialRuntimeSlot) == 40,
                  "MaterialRuntimeSlot 行宽已冻结（B5-b）：改字段必须同步改断言并回写计划文档");

    class MaterialRuntimeTable
    {
    public:

        /// 容量上限；超限即 fail-fast（不静默扩容）。依据见文件头"预算制"。
        static constexpr uint32_t kMaterialRuntimeCapacityLimit = 4096;

    private:

        std::vector<MaterialRuntimeRow> rows;

        /// 已回收的行号（复用按世代 `+2`）。
        std::vector<MaterialRuntimeRowID> free_rows;

        /// 索引键 = **整个 MaterialRuntimeKey**（不是单维哈希）：碰撞也不误合并。
        hgl::UnorderedMap<MaterialRuntimeKey, MaterialRuntimeRowID,
                          MaterialRuntimeKeyHash> index_by_key;

        /// 每实例 slot（按实体稀疏存放）。std::unordered_map 保证元素**引用稳定**。
        std::unordered_map<EntityID, MaterialRuntimeSlot> slots;

        /// 所属世界（退休 GPU 侧绑定用；单测里为 nullptr ⇒ 跳过 GPU 动作）
        ECSContext *context = nullptr;

        uint32_t live_row_count = 0;

        /// 超限告警只打一次（预算制的"不刷屏"要求）。
        uint32_t overflow_warn_count = 0;

        /// 取一个可用行号（free list 优先；新行世代从 1 起，复用 +2 保持奇数）。
        /// 超容量且无空闲行 ⇒ 返回 INVALID（调用方负责告警）。
        MaterialRuntimeRowID AllocateRow();

        /// 回收一行：退休 GPU 绑定 → 摘索引 → 清内容（保留世代值供复用 +2）→ 入 free list。
        void RecycleRow(MaterialRuntimeRowID id);

    public:

        MaterialRuntimeTable() = default;

        /// 析构**不**回收 GPU 绑定（世界析构期 GraphicsContext 可能已/将销毁；
        /// 池资源由 GraphicsContext 侧的注册表统一释放）。
        ~MaterialRuntimeTable() = default;

        MaterialRuntimeTable(const MaterialRuntimeTable &) = delete;
        MaterialRuntimeTable &operator=(const MaterialRuntimeTable &) = delete;

        /// 绑定所属世界（世界 ctor 里调用，同 `VisibilityDataStorage::SetContext`）。
        void SetContext(ECSContext *ctx) { context = ctx; }

        // ── 共享行：intern / refcount ──

        /// 同键去重：已登记 ⇒ 返回原行（**refcount +1**）；新键 ⇒ 登记新行（refcount = 1）。
        /// 超容量上限且无空闲行 ⇒ 返回 `INVALID_MATERIAL_RUNTIME_ROW_ID` 并（仅首次）报错。
        MaterialRuntimeRowID Intern(const MaterialRuntimeKey &key);

        /// 显式加一次引用（同一行被第二个 slot 引用时用）。
        MaterialRuntimeRowID AddRef(MaterialRuntimeRowID id);

        /// 释放一次引用；**返回 true 表示该行因归零被回收**（GPU 绑定已退休）。
        bool Release(MaterialRuntimeRowID id);

        /// 写时分裂：把引用升级为**自有行**。
        ///   · 已是自有行 ⇒ 原样返回原行号（幂等）；
        ///   · 独占的共享行（refcount <= 1）⇒ **就地**转自有（摘键索引）；
        ///   · 被多处引用的共享行 ⇒ 复制出一条**自有行**（不在键索引里，refcount = 1），
        ///     源行 refcount -1；返回新行号（超容量 ⇒ INVALID）。
        /// 写时分裂（CoW）：把实例的行引用换成一条**不入键索引的自有行**
        /// （已是自有行 ⇒ 幂等；独占行 ⇒ 就地转自有、零拷贝；被多处引用 ⇒ 拷贝 + 对源行 `Release`）。
        /// ⚠ 生产路径当前未调用；用途与使用条件见文件头「写时分裂」条（2026-10-10 审计复核）。
        MaterialRuntimeRowID DetachOwned(MaterialRuntimeRowID id);

        /// 注意：`Get`/`GetMutable` 返回的指针在下一次 `Intern`/`DetachOwned`/`Clear`
        /// 之前有效（`rows` 是连续容器，分配可能触发扩容搬家）。
        const MaterialRuntimeRow *Get(MaterialRuntimeRowID id) const;
        MaterialRuntimeRow *GetMutable(MaterialRuntimeRowID id);

        uint32_t GetRefCount(MaterialRuntimeRowID id) const;

        /// 行世代（句柄失效检测）。死行/越界返回 0（`0 = 死` 约定）。
        uint32_t GetGeneration(MaterialRuntimeRowID id) const;

        /// 活行总数（共享行 + 自有行）
        uint32_t GetCount() const { return live_row_count; }

        /// 共享行数（在键索引里的活行）
        uint32_t GetSharedRowCount() const;

        /// 自有行数（CoW 分裂出来的活行）
        uint32_t GetOwnedRowCount() const;

        /// 空闲行数（可复用行号）
        uint32_t GetFreeRowCount() const { return static_cast<uint32_t>(free_rows.size()); }

        /// 超限告警次数（诊断 + 测试用：验证"只告警一次"）
        uint32_t GetOverflowWarnCount() const { return overflow_warn_count; }

        // ── GPU 侧绑定 ──

        /// 退休某行的纹理配置池行（只做 CPU 侧记账；行归零回收时自动调用，
        /// 也供"就地重置物化绑定"复用）。世界不可用 / 无有效分配 ⇒ no-op。
        void RetireRowTextureConfiguration(MaterialRuntimeRow &row);

        // ── 每实例 slot ──

        /// 实体 → 运行期 slot（无则创建）。命名向 `ECSContext::GetOrCreateMaterialData` 看齐；
        /// ⚠ stage B 会换成**值类型句柄**（与 TransformAccessor / BoundingBoxAccessor 同构），
        /// 届时不再返回引用。
        MaterialRuntimeSlot &GetOrCreateSlot(EntityID owner);

        /// 实体 → 运行期 slot（无则 nullptr）
        MaterialRuntimeSlot *GetSlot(EntityID owner);
        const MaterialRuntimeSlot *GetSlot(EntityID owner) const;

        /// 销毁某实体的 slot（**释放它持有的行引用**；行归零则触发回收）。存在则返回 true。
        bool DestroySlot(EntityID owner);

        uint32_t GetSlotCount() const { return static_cast<uint32_t>(slots.size()); }

        /// 清空（行 + slot + 索引 + 告警闩；行回收会退休其 GPU 绑定）
        void Clear();
    };
}//namespace hgl::ecs
