/**
 * MaterialVariantTable.h —— **材质变体表**（v2 §9.3 A3）
 *
 * 用途：把 `MaterialComponent` 里**前向/阴影两个硬编码 program 槽**收敛成
 *       「一张按**静态键**去重的表 + 每实体两个变体 ID」：
 *         · 变体记录按**静态键**去重——同键的实体共享同一条记录（program 由
 *           `ShaderProgramManager` 缓存持有并 refcount，记录只持引用）；
 *         · 组件只持 ID，解析/比对/失效/取用全部走表，没有第二份 program 真值。
 *
 * 键的规矩（**不可越界**，A4/A5 扩展时务必遵守）——判据只有一条：
 *   **这一维是否决定 program 身份**。决定 ⇒ 必须入键（否则两个不同 program 撞
 *   同一条记录、被后写覆盖）；不决定 ⇒ 绝不能入键（放进去只会让表无界膨胀）。
 *
 *   · **必须入键**（两维，缺一不可）：
 *       ① `program_build_context` = `graph::mtl::HashMaterialProgramBuildContext(
 *          primitive_type, 顶点格式, 设备 profile, purpose)`（purpose 已区分
 *          ForwardColor / ShadowDepth / DepthOnly ⇒ 前向与阴影天然落在不同键上，
 *          不会互相驱逐）；
 *       ② `recipe_hash` = **材质侧**决定 program 身份的那一维（当前 = 对本次解析
 *          所用 effective recipe 求 `graph::mtl::HashMaterialRecipe`）。**同一
 *          build context 下不同材质必须落不同记录**——只按 ① 去重会让两种材质共用
 *          一条记录，后解析者的 program 覆盖先解析者（先解析者拿到别人的 program）。
 *          A3 之前"是否需要重解析"的判据就是 `(recipe_hash, build_context_hash)`
 *          两量同时比对，这里把该身份固化进键。
 *          更贴切的取值本是已构造好的 `mtl::ShaderProgramKey` 摘要（fragment stage
 *          digest 才是 program 真身份），但它只能在 `AcquireShaderProgram` 成功
 *          之后经 `ShaderProgram::GetProgramKey()` 拿到（键在
 *          `src/SceneGraph/module/ShaderProgramManager.cpp` 内部构造，acquire 前
 *          不暴露）；而阴影侧登记变体**必须早于 acquire**（失败也要有 retry_frames
 *          归属，见下"阴影侧时序"），故本步取 recipe 哈希。
 *          recipe 哈希只会**多分裂**（两条 recipe 恰好生成同一 program ⇒ 两条记录
 *          指向同一个 program 指针），**不会少分裂**；少分裂才是 bug。
 *   · **绝不能入键**（不决定 program 身份，且每帧/每实例变化）：**LOD 档选择**、
 *     **质量档 / 画质切换**、**每实例选择器**、材质实例参数（纹理绑定 / data_index）、
 *     帧号、相机、可见性……放进去会让表随实体数/帧数无界增长、每帧制造新记录
 *     （正是 v2 §9.3 要避免的形态）。每实例选择器属**实例侧**，A4 会在实例侧解决。
 *   · **键必须静态且有界**：只允许放"一帧之内不变"的维度（① 是有限枚举的乘积；
 *     ② 的取值集合受材质数上界约束）。
 *   · 若未来要把 `MaterialRecipe::compile_defines` 入键，**必须先归一化再哈希**
 *     （去序 / 去重 / trim——它当前是 `std::vector<std::string>`，本身无界，
 *     直接入键等于把无界集合塞进键），且仍不得放每帧维度。
 *
 * 阴影侧时序（A3 沿用、A4 需遵守）：阴影变体登记（`Intern`）必须发生在
 *   `AcquireShaderProgram` **之前**——模板选择 / 编译失败时也要有可记账的
 *   retry_frames 归属（D9）。因此键的第二维必须能在 acquire 之前算出 ⇒ 用 recipe
 *   哈希，而不是 acquire 之后才有的 `ShaderProgramKey`。
 *
 * 归属：表由 `ECSContext` 持有（与 `VisibilityDataStorage` / `RenderItemDataStorage`
 *       等世界私有存储同一访问范式，见 `Context.h::GetMaterialVariantTable()`）。
 *       理由：记录里的 `program` 指针归该世界的 `GraphicsContext` 的
 *       `ShaderProgramManager` 缓存所有——表与程序缓存同生共死，不会出现
 *       "进程级全局表持有已销毁 GraphicsContext 的 program"的悬垂指针，也不会让
 *       两个世界（不同 GraphicsContext）互相污染同一批记录。
 *
 * 预算制：容量上限 `MaterialVariantTable::kMaterialVariantCapacityLimit`，超限
 *       **fail-fast**（返回 `INVALID_MATERIAL_VARIANT_ID`）并**只告警一次**——
 *       不静默扩容。依据：键的每个维度都是有限枚举的乘积（primitive_type ×
 *       顶点格式 × 设备 profile × purpose × 材质配方身份），真实场景里不同变体数
 *       在个位数到几十；
 *       4096 给出两个数量级余量，同时把表内存上限锁在 ~128KB（记录 ~32B/条）。
 */
#pragma once

#include<cstdint>
#include<cstddef>
#include<vector>
#include<hgl/type/UnorderedMap.h>
#include<hgl/util/hash/FNV1a.h>

namespace hgl::graph
{
    class ShaderProgram;
}

namespace hgl::ecs
{
    /// 变体句柄：表内下标；`INVALID_MATERIAL_VARIANT_ID` 表示"未登记/登记失败"。
    using MaterialVariantID = uint32_t;

    inline constexpr MaterialVariantID INVALID_MATERIAL_VARIANT_ID =
        static_cast<MaterialVariantID>(-1);

    /// 变体键：**静态且有界**（见文件头"键的规矩"）。
    /// 两维都"决定 program 身份"，缺任一维都会让不同 program 撞同一条记录：
    ///   · 缺 ① 分不清 purpose / 图元 / 顶点格式 / 设备 profile；
    ///   · 缺 ② 分不清材质（同 build context 下不同材质 ⇒ program 互相覆盖）。
    struct MaterialVariantKey
    {
        /// ① build context hash = primitive_type + 顶点格式 + 设备 profile + purpose。
        /// 不许再塞每帧/每实例维度；可变维度要入键必须先归一化再哈希。
        uint64_t program_build_context = 0;

        /// ② 材质侧 program 身份 = 本次解析所用 effective recipe 的哈希
        /// （`graph::mtl::HashMaterialRecipe`）。同 ① 的约束：静态、有界、先归一化。
        uint64_t recipe_hash = 0;

        bool operator==(const MaterialVariantKey &rhs) const noexcept
        {
            return program_build_context == rhs.program_build_context
                && recipe_hash == rhs.recipe_hash;
        }
    };

    /// 键哈希（**仅**用于内部索引）。相等性仍是全维度 `operator==`：哈希碰撞不会让
    /// 两个不同键共用记录（ankerl map 哈希相等时仍逐个比对 key）。
    struct MaterialVariantKeyHash
    {
        std::size_t operator()(const MaterialVariantKey &key) const noexcept
        {
            hgl::hash::FNV1aHasher64 h;
            h << key.program_build_context
              << key.recipe_hash;
            return static_cast<std::size_t>(h.Result());
        }
    };

    /// 变体记录：同键的实体**共享**同一条记录。
    ///   · `program` 只在解析路径写（解析成功后落表；解析失败只清该键的引用），
    ///     **不得**在渲染热路径每帧无条件写——那会污染同键的其它实体。
    ///   · `retry_frames` 是 D9 的跳过收敛计数，语义 = **该变体**的连续跳过帧数
    ///     （原先是 MaterialComponent 上的每实体一份，A3 随槽位一起收敛到记录）。
    ///     它只能由解析/跳过判定路径写，成功产出 render item 时复位。
    ///     ⚠ **A4 会把重试/降频状态移到每实例侧**（共享记录不得承载每实例状态）：
    ///     同键的健康兄弟每帧复位，会持续清零失败者的计数，可能掩盖 D9 的 masked
    ///     失败告警/降频——**只影响诊断，不影响渲染**，A4 随每实例侧一并解决。
    struct MaterialVariantRecord
    {
        MaterialVariantKey key{};
        hgl::graph::ShaderProgram *program = nullptr;
        uint32_t retry_frames = 0;

        /// 保留给 A4/A5 的生命周期位；**本步不写**——它们是"每实体/每帧"语义，
        /// 写进共享记录会跨实体污染（见文件头"键的规矩"的"绝不能入键"一节）。
        bool pending = false;
        bool failed = false;
    };

    class MaterialVariantTable
    {
    public:

        /// 容量上限；超限即 fail-fast（不静默扩容）。依据见文件头"预算制"。
        static constexpr uint32_t kMaterialVariantCapacityLimit = 4096;

    private:

        std::vector<MaterialVariantRecord> records;

        /// 索引键 = **整个 MaterialVariantKey**（不是单维哈希）：碰撞也不误合并。
        hgl::UnorderedMap<MaterialVariantKey, MaterialVariantID,
                          MaterialVariantKeyHash> index_by_key;

        /// 超限告警只打一次（预算制的"不刷屏"要求）。
        uint32_t overflow_warn_count = 0;

    public:

        MaterialVariantTable() = default;
        ~MaterialVariantTable() = default;

        MaterialVariantTable(const MaterialVariantTable &) = delete;
        MaterialVariantTable &operator=(const MaterialVariantTable &) = delete;

        /// 同键去重（键 = build context **且** recipe 两维全同）：已登记 ⇒ 返回原
        /// ID（不新增记录）；新键 ⇒ 登记并返回新 ID。
        /// 超容量上限 ⇒ 返回 `INVALID_MATERIAL_VARIANT_ID` 并（仅首次）报错。
        MaterialVariantID Intern(const MaterialVariantKey &key);

        /// 注意：`Get`/`GetMutable` 返回的指针在下一次 `Intern`/`Clear` 之前有效
        /// （`records` 是连续容器，`Intern` 可能触发扩容搬家）。
        const MaterialVariantRecord *Get(MaterialVariantID id) const;
        MaterialVariantRecord *GetMutable(MaterialVariantID id);

        uint32_t GetCount() const { return static_cast<uint32_t>(records.size()); }

        /// 超限告警次数（诊断 + 测试用：验证"只告警一次"）。
        uint32_t GetOverflowWarnCount() const { return overflow_warn_count; }

        void Clear();
    };
}//namespace hgl::ecs
