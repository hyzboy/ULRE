#pragma once

// MaterialAuthoringSideTable.h — 材质授权记录 `MaterialData` 本体 + 世界级存储（2026-11 定稿）
//
// 为什么长这样：`MaterialData` 承载**每实体**的授权状态（配方覆盖 + 具名纹理覆盖 +
// 内容行绑定引用 + 授权代数），里面装着 `MaterialRecipe`（含 `AnsiString`/`U8String`/
// `hgl::ArrayList`）与 `hgl::UnorderedMap` —— 全都不是 POD，不该进任何设备侧行 arena。
// 2026-11 两步退役后本文件成为唯一真源：
//   · 第一步：`GlobalSSBOType::MaterialData` 设备级组件行池整体删除（GPU 从未消费，
//     1024 行预算反而给每实体场景造成「行池分配失败」假错误）；
//   · 第二步：`ComponentType::MaterialData` ECS 组件退役 —— 组件层只是旁表的转发壳，
//     实体只要被收集就先得建壳再建条目，纯多余。
// ⇒ `MaterialData` 收敛为**纯授权记录**：不再继承 `ComponentRow`、不占类型表槽位；
//   记录直接住在这张世界级旁表里，**键 = `EntityID`**，实体销毁时由世界清理。
//   取用口保持原名（`ECSContext::Get/CreateMaterialData`），语义改为「查/建旁表记录」：
//   从未做过材质授权 ⇒ `Get` 返回 nullptr（普通实体零内存）。

#include<hgl/CoreType.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>       // hgl::graph::GlobalSSBOBinding（内容行三元组）
#include<hgl/graph/ssbo/MaterialSSBOLayout.h>    // SetMaterialDataRow 的行类型特征/版本
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h> // hgl::graph::GlobalSSBODataAccessor
#include<hgl/ecs/core/entity/EntityHandle.h>     // hgl::ecs::EntityID（键类型）
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
#include<cstdint>
#include<string>

namespace hgl
{
    namespace graph
    {
        class ShaderProgram;
        class Texture;
    }
}

namespace hgl::ecs
{
    class ECSContext;
    /// 具名纹理资源的**选择器**类型（B4-iii-b：原为 `MaterialData` 的嵌套枚举，随重类型移出）。
    /// 在 `hgl::ecs` 域内保持同名 ⇒ 组件与调用点的不限定写法零改动。
    enum class MaterialTextureResourceKind : uint8_t
    {
        Texture2D = 0,
        Texture2DArray
    };

    /// 具名纹理授权资源（B4-iii-b：原为 `MaterialData` 的嵌套类型；随重类型整体移入旁表，
    /// 组件不再持有它 —— 类型名与限定名在 `hgl::ecs` 内保持不变，调用点零改动）。
    ///
    /// **统一句柄（2026-10 定稿）**：纹理来源只有一个字段 `asset`（`TextureAsset*`）。
    /// 未加载时句柄停在 `MetaOnly`（只带文件名），已加载时带完整 `Texture`；作者不再需要
    /// 在「自己先同步加载」与「异步回填」之间二选一 —— ECS 侧判断 `IsReady()`，未就绪就自动
    /// 驱动异步加载，等就绪后再让图元进入可渲染状态。
    ///
    /// **没有 `sampler` 字段（2026-10-29 解耦）**：Vulkan 侧是 Bindless —— 采样器由
    /// `contract/sampler.toml` 的预设集中写入 `binding=1` 的 `samplers[]`，片元着色器按预设名字
    /// 索引采样；材质侧传进来的 `Sampler*` 从未进入任何描述符（旧描述符时代残留）。
    /// 因此本结构只表达「哪张纹理」，采样状态不是每纹理的授权数据。
    struct MaterialTextureAuthoringResource
    {
        /// 纹理句柄（由 `TextureManager` 的句柄表拥有；本结构只持裸观察指针）
        hgl::graph::TextureAsset *asset = nullptr;
        /// 注：kind/array_layer/required 是**选择器**（不是每实体 GPU 数据）——
        /// 留在旁表条目里即可，不进设备级行。
        MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D;
        uint32_t array_layer = 0;
        bool required = false;

        /// 句柄是否带了可绑定的纹理（= 已就绪）
        hgl::graph::Texture *GetTexture() const
        {
            return asset ? asset->GetTexture() : nullptr;
        }

        /// 句柄是否带了**可用内容**（= 容器就绪且内容就绪）
        ///
        /// RT 来源的句柄在附件纹理创建后就已经有 `GetTexture()`，但此时离屏 pass 还
        /// 没画过任何东西。若这里只看容器，主世界会在离屏 pass 产出内容之前就采样
        /// 一张只有清屏色的纹理，且此后永远不重画 ⇒ 改用句柄的 `IsReady()`。
        bool IsReady() const { return asset && asset->IsReady(); }

        /// 是否表达了「某个纹理」（已就绪的纹理，或只有来源待加载/待绘制）
        bool HasSource() const
        {
            return asset && (asset->GetTexture() || !asset->GetFilename().IsEmpty());
        }

        /**
         * @brief 句柄是否**永久失败**（`LoadFailed`）。
         *
         * `TextureStreamingSystem` 明确不对 `LoadFailed` 自动重试（坏文件每帧重读只会刷屏），
         * 因此这个状态在本进程内是**终态**。渲染侧据此把「等它好」降级为「当它不在」：
         * 该槽位的 bindless 句柄保持 0，由 GLSL 侧 `SampleOptional()` /
         * `if (handle == 0u)` 的 fallback 兜底（白色基色、未扰动法线），
         * 而不是让整条材质链失败、把整个图元永久藏在收集之外。
         */
        bool IsTerminallyFailed() const
        {
            return asset
                && !IsReady()
                && asset->GetPayloadState() == hgl::PayloadState::LoadFailed;
        }
    };

    /// 必需纹理的**就绪结论**（三值互斥）。唯一真源：句柄 `PayloadState`。
    enum class RequiredTextureReadiness : uint8_t
    {
        Ready = 0,      ///< 全部就绪（或没有必需纹理）
        Pending,        ///< 至少一张仍在读盘/上传 ⇒ 图元暂不渲染
        Degraded,       ///< 无在途任务，但至少一张已永久失败 ⇒ 句柄 0 占位保底渲染
    };

    /**
     * MaterialData —— 材质**授权记录**（v2 §9.3 三层中的第一层；2026-11 起不再是 ECS 组件）。
     *
     * 职责：承载作者授权的材质参数（配方覆盖 / 具名纹理资源 / 材质数据行资源），
     * 资源级、按指纹去重。**不含**运行期解析结果（MaterialVariant）与每实例绑定
     * （MaterialRuntime）——那两层分别由解析流程与**材质运行期共享行/每实例 slot**
     * （support/MaterialRuntimeTable.h）承担。
     *
     * 解析出口只有一个：`BuildResolvedRecipe`。asset 里的默认配方是**基底**，
     * 本记录的配方覆盖是**覆盖源**；参数按声明逐条校验后产出规范化结果。
     *
     * 存储形态：本记录是**纯授权数据**——不继承 `ComponentRow`、不占 ComponentType
     * 槽位，直接住在世界级 `MaterialAuthoringSideTable`（键 = `EntityID`）里；
     * `entity_id`/`context` 由旁表在建记录时写入。从未授权 ⇒ 旁表里根本没有这条记录
     * （`GetMaterialData` 返回 nullptr），普通实体零材质内存。
     */
    class MaterialData
    {
        friend class MaterialAuthoringSideTable;

    public:
        struct MaterialDataAuthoringResource
            : hgl::graph::GlobalSSBOBinding
        {
            // B4-i：GPU 面只剩**类型 + 行号**（基类 `GlobalSSBOBinding`）——原先还挂着每实体的
            // 缓冲指针 + 元素容量 + 字节步长三个**死字段**（实测外部读者 0、示例赋值 0 处；容量与
            // 步长的真源在 `SSBOBufferRegistry::RegisterBuffer`，由字节数 ÷ 容量反算步长）
            // ⇒ 与 B3-iii「删几何三指针」同构，整组删除；名字不再出现，契约按「零残留」判。
            // 2026-10-10（审计 §1.1）：字段 `authored` 已删除 —— `.authored` 全仓**读写 0 处**，
            // 与本结构 B4-i 自己立的「零残留」判据（GPU 面只剩类型 + 行号）矛盾；上面那句注释里的
            // 三个死字段同批被判过。Test 31 的对应 needle 已由「必须存在」改为「禁复活」
            // （⚠ 注释里不得再出现该声明原文，否则会撞上那条禁复活针）。

            MaterialDataAuthoringResource() = default;
            MaterialDataAuthoringResource(
                const MaterialDataAuthoringResource &) = default;
            MaterialDataAuthoringResource(
                const hgl::graph::GlobalSSBOBinding &binding) noexcept
                : hgl::graph::GlobalSSBOBinding(binding)
            {
            }

            MaterialDataAuthoringResource &operator=(
                const MaterialDataAuthoringResource &) = default;

            MaterialDataAuthoringResource &operator=(
                const hgl::graph::GlobalSSBOBinding &binding) noexcept
            {
                hgl::graph::GlobalSSBOBinding::operator=(binding);
                return *this;
            }

            hgl::graph::GlobalSSBOBinding
                GetGlobalSSBOBinding() const noexcept
            {
                return {ssbo_type, ssbo_id, data_index};
            }
        };

    public:

        // ── 授权数据（唯一真源 = 本记录；住世界级旁表，键 = EntityID）──
        bool has_recipe_override = false;
        hgl::graph::mtl::MaterialRecipe recipe_override;
        hgl::UnorderedMap<hgl::AnsiString, MaterialTextureAuthoringResource>
            named_texture_resources;

        // ── 2026-11 从退役的 MaterialData 设备组件行吸收的字段 ──────────────
        /// 授权的内容行三元组（= 旧组件行 surface_type/ssbo_id/data_index）。
        /// `has_data_resource == false` 时本字段无意义。
        hgl::graph::GlobalSSBOBinding data_resource{};
        bool has_data_resource = false;
        /// 授权代数（= 旧组件行高 24 位 revision 的行内镜像）。每次授权变更 ++，
        /// 收集路径据此判断「配方是否需要在下一帧重解析」。
        uint32_t authored_generation = 0;
        uint64_t recipe_generation = 0;

    public:

        MaterialData() = default;

        /// 记录所属实体（建记录时由旁表写入；默认构造 = 无效 id）
        EntityID GetEntityID() const { return entity_id; }
        /// 记录所属世界（同上；从未挂上世界的记录 = nullptr）
        ECSContext *GetContext() const { return context; }

        // ── 配方覆盖 ──
        void SetRecipe(const hgl::graph::mtl::MaterialRecipe &recipe);

        /// 撤销配方覆盖，回到「asset 默认配方」。
        ///
        /// 与 `SetRecipe({})` 不同，此操作不会留下一个空的覆盖。
        void ClearRecipeOverride();

        const hgl::graph::mtl::MaterialRecipe *GetRecipeOverride() const;
        bool HasRecipeOverride() const { return GetRecipeOverride() != nullptr; }
        uint64_t GetRecipeGeneration() const { return recipe_generation; }

        // ── 具名纹理资源 ──
        /// **统一入口**：`resource.asset` 是唯一纹理来源（`TextureAsset*` 句柄）。
        /// 句柄只有文件名（`MetaOnly`）时也接受 —— ECS 侧会自动驱动加载，就绪后再渲染。
        bool SetTextureResource(const std::string &name,
                                const MaterialTextureAuthoringResource &resource);

        /// 便捷重载：把一张**已就绪**的纹理包成句柄后走同一个统一入口
        /// （`WrapTextureAsset` 不接管纹理所有权，纹理仍归 `TextureManager`）。
        /// 它不构成第二条行为路径 —— 落库的同样是一个 `TextureAsset*`。
        ///
        /// 采样器由 Vulkan Bindless 预设（`contract/sampler.toml` → `samplers[]`）统一管理，
        /// 材质侧**不存在**采样器参数（2026-10-29 起彻底移除）。
        bool SetTextureResource(const std::string &name,
                                hgl::graph::Texture *texture,
                                MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D,
                                uint32_t array_layer = 0,
                                bool required = false);

        /// 便捷重载：直接给**句柄**。句柄可以是「已就绪」，也可以只有文件名
        /// （`MetaOnly`）—— 后者由 `TextureStreamingSystem` 自动加载，此处不做任何判断，
        /// 与统一入口完全同义，只是省去手填 `MaterialTextureAuthoringResource`。
        ///
        /// 注意：纹理的推荐归属地是**配方**（`MaterialRecipe::AddTexture`）；本入口只用于
        /// 「同一配方下少数实例换图」的同名覆盖场景。
        bool SetTextureResource(const std::string &name,
                                hgl::graph::TextureAsset *asset,
                                MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D,
                                uint32_t array_layer = 0,
                                bool required = false);

        /// 查具名纹理资源。返回条目只要求「表达了某个纹理」（已就绪或只有文件名）；
        /// 是否可绑定由调用方看 `resource.GetTexture()`/`IsReady()`。
        const MaterialTextureAuthoringResource *GetTextureResource(const std::string &name) const;

        /// 是否仍有**必需的**纹理句柄未就绪（`required && !IsReady()`）。
        /// 渲染决策据此判定「资源未加载完成时不渲染」：任一必需贴图缺席 ⇒ 整个图元暂不进入收集。
        /// 就绪度直接读句柄的 `PayloadState`，不另立计数器 ⇒ 无双状态源。
        ///
        /// **不含永久失败**（Step 5.2 占位保底）：`LoadFailed` 是终态、不会自己好，
        /// 若照样把图元钉在不可渲染，一个坏文件就能让整个图元从此消失。
        /// 这类句柄改由 `HasFailedRequiredTexture()` 单独报告，渲染侧以句柄 0 占位保底。
        bool HasPendingRequiredTexture() const;

        /// 是否存在**永久失败**的必需纹理（`required && IsTerminallyFailed()`）。
        ///
        /// 与 `HasPendingRequiredTexture()` 互斥：仍在途 ⇒ 前者 true；已定性失败（无在途任务）
        /// ⇒ 本判据 true。渲染侧对后者不再等待，直接把该槽位留作句柄 0 走 fallback。
        bool HasFailedRequiredTexture() const;

        /// 必需纹理的**就绪结论**（三值互斥；见 `EvaluateRequiredTextureReadiness(record)`）。
        /// 规则本体住在旁表头文件里（无设备依赖）⇒ 可被单元测试直接钉住。
        RequiredTextureReadiness EvaluateRequiredTextureReadiness() const;

        // ── 材质数据行资源（内容行三元组）──
        /// 授权 / 清空本实体引用的**内容行**（PBR/自发光/透射…池里的行）。
        /// `resource.ssbo_id == 0` ⇒ 清空；其余情况要求 binding 有效（无效即报错拒绝）。
        /// 真源写在本记录的 `data_resource` 字段；每次变更推进授权代数。
        bool SetDataResource(const MaterialDataAuthoringResource &resource);

        /// 是否已绑上材质数据行（未授权 ⇒ false）。
        bool HasDataResource() const;

        /// Write a typed material row through a caller-owned lease and bind it.
        /// The accessor must outlive this record's use of the bound row.
        template<typename T>
        bool SetMaterialDataRow(
            hgl::graph::GlobalSSBODataAccessor &accessor,
            const T &data)
        {
            constexpr hgl::graph::GlobalSSBOType type =
                hgl::graph::ssbo::GlobalRowTypeTraits<T>::TYPE;
            static_assert(
                hgl::graph::GetGlobalSSBOTypeStructVersion(type) != 0,
                "SetMaterialDataRow requires a registered material row type");

            if (!accessor
             || accessor.GetGlobalSSBOType() != type
             || accessor.GetRowBytes() != sizeof(T))
            {
                GLogError(
                    "[MaterialData] typed row lease mismatch type=%s bytes=%u expected=%zu",
                    hgl::serialize::EnumNameOf(type),
                    accessor.GetRowBytes(),
                    sizeof(T));
                return false;
            }

            if (!accessor.Write(data))
                return false;

            MaterialDataAuthoringResource resource{};
            resource = accessor.GetGlobalSSBOBinding();
            return SetDataResource(resource);
        }

        /// 授权代数（每次授权状态变化 +1；真源 = 本记录 `authored_generation` 字段）。
        /// 每实例 slot 的跟踪副本（`MaterialRuntimeSlot::tracked_material_data_generation`）
        /// 与之比对，相等即跳过整条解析链。
        /// （2026-11：原 `GetAuthoredRowRevision()` 读设备组件行的行内 revision —— 组件行池
        /// 退役后该名字失去所指，代数读数回归 `GetAuthoredGeneration()`。）
        uint32_t GetAuthoredGeneration() const;

        /// 上游来源（asset 默认配方 / 变体切换）变化时前进授权代数：
        /// 只动代数，不改授权数据——下游据此让解析结果失效。
        /// 同时记一次 `TopologyChange::MaterialBinding`（P1：材质批键可能变）。
        void BumpAuthoredGeneration();

        // ── 解析（本层唯一出口）──
        /// @param asset_default_recipe asset 里的默认配方（基底；可为 nullptr）
        bool BuildResolvedRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                 const hgl::graph::ShaderProgram *material_program,
                                 const hgl::graph::mtl::MaterialRecipe *asset_default_recipe) const;

    private:

        /// 授权变更**唯一入口**：代数 +1 并记一次 TopologyChange::MaterialBinding。
        /// 所有授权入口（SetRecipe / SetTextureResource / SetDataResource）都必须走它。
        void TouchAuthored();

        EntityID entity_id{};       ///< 归属实体（旁表建记录时写入）
        ECSContext *context = nullptr;  ///< 归属世界（同上）
    };

    /**
     * 材质授权**旁表**（`MaterialData` 记录的世界级存储）。
     *
     * 为什么存在：配方覆盖（`mtl::MaterialRecipe`）、具名纹理资源（`UnorderedMap<AnsiString, …>`）
     * 是重类型 ⇒ 不能进任何设备侧行 arena；它们本来就只被 CPU 侧的收集路径消费。
     * **键 = `EntityID`**。
     *
     * 归属：**世界级**（`ECSContext` 持有，与 MaterialVariantTable / MaterialRuntimeTable 同域）——
     * 授权数据只被本世界的渲染路径消费。
     *
     * 生命周期：**随实体死亡而摘除**（`~Entity` 调 `Remove`；替代原 `MaterialData::OnDetach`）。
     */
    class MaterialAuthoringSideTable
    {
        friend class ECSContext;

        void SetOwnerContext(ECSContext *ctx) { owner_context = ctx; }

    public:

        /// 挂一个世界（建记录时写入记录的 context 字段）。默认构造仅服务于
        /// 「独立于世界的记录装配」测试场景（context 留空 ⇒ 授权变更不推拓扑位）。
        explicit MaterialAuthoringSideTable(ECSContext *owner_context = nullptr)
            : owner_context(owner_context)
        {
        }

        /// 取（无则建）实体对应的授权记录；非法 EntityID ⇒ nullptr
        MaterialData *GetOrCreate(EntityID entity_id);

        /// 查记录（无则 nullptr）
        MaterialData *Find(EntityID entity_id);
        const MaterialData *Find(EntityID entity_id) const;

        /// 摘除记录（实体死亡时调用）；返回是否真的摘到
        bool Remove(EntityID entity_id);

    private:

        hgl::UnorderedMap<EntityID, MaterialData> entries;
        ECSContext *owner_context = nullptr;
    };

    /**
     * @brief 由授权记录算出「必需纹理」的就绪结论。
     *
     * 单独成函数（不吃设备、不吃行号）是为了让渲染门禁的判定规则可被单元测试直接钉住
     * —— 这条规则一旦写错，坏一个贴图就能让整个图元永久消失，且现场只表现为「东西不见了」。
     *
     * @param record 授权记录（`nullptr` ⇒ 没有任何授权资源 ⇒ Ready）
     */
    inline RequiredTextureReadiness EvaluateRequiredTextureReadiness(
        const MaterialData *record)
    {
        if (!record)
            return RequiredTextureReadiness::Ready;

        // 单趟同时算出两种结论（互斥，优先报「仍在途」）：
        //   · 有一张既没就绪、也没定性失败 ⇒ 等它，本帧不渲染；
        //   · 全部有了结论，但至少一张终态失败 ⇒ 降级：句柄 0 占位保底渲染。
        bool failed = false;

        for (const auto &[named_key, resource] : record->named_texture_resources)
        {
            // 只对「必需 + 确实声明了来源」的条目把关：既无纹理又无文件名的空条目
            // 本来就不会被收集路径当作有效绑定，不该把图元永久钉在不可渲染状态。
            if (!resource.required || !resource.HasSource() || resource.IsReady())
                continue;

            if (resource.IsTerminallyFailed())
            {
                failed = true;
                continue;
            }

            return RequiredTextureReadiness::Pending;
        }

        return failed ? RequiredTextureReadiness::Degraded
                      : RequiredTextureReadiness::Ready;
    }

    inline MaterialData *
        MaterialAuthoringSideTable::GetOrCreate(const EntityID entity_id)
    {
        if (!entity_id.IsValid())
        {
            GLogError("[MaterialAuthoringSideTable] 拒绝非法 EntityID（index=0x%08X）",
                      entity_id.index);
            return nullptr;
        }

        if (MaterialData *exist = entries.GetValuePointer(entity_id))
            return exist;

        MaterialData record{};
        record.entity_id = entity_id;
        record.context = owner_context;

        if (!entries.Add(entity_id, record))
        {
            GLogError("[MaterialAuthoringSideTable] 旁表分配失败 entity=%u（内存不足？）",
                      entity_id.index);
            return nullptr;
        }

        return entries.GetValuePointer(entity_id);
    }

    inline MaterialData *
        MaterialAuthoringSideTable::Find(const EntityID entity_id)
    {
        return entity_id.IsValid() ? entries.GetValuePointer(entity_id) : nullptr;
    }

    inline const MaterialData *
        MaterialAuthoringSideTable::Find(const EntityID entity_id) const
    {
        return entity_id.IsValid() ? entries.GetValuePointer(entity_id) : nullptr;
    }

    inline bool MaterialAuthoringSideTable::Remove(const EntityID entity_id)
    {
        return entity_id.IsValid() ? entries.DeleteByKey(entity_id) : false;
    }
}//namespace hgl::ecs
