#pragma once

#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
#include <hgl/log/Log.h>
#include<cstdint>
#include<string>

// Forward declarations to avoid heavy includes
namespace hgl
{
    namespace graph
    {
        class GlobalSSBOBufferRegistry;
        class Sampler;
        class ShaderProgram;
        class Texture;
        class TextureAsset;
    }
}

namespace hgl::ecs
{
    /**
     * MaterialData —— 材质**数据层**（v2 §9.3 三层中的第一层）。
     *
     * 职责：承载作者授权的材质参数（配方覆盖 / 具名纹理资源 / 材质数据行资源），
     * 资源级、按指纹去重。**不含**运行期解析结果（MaterialVariant）与每实例绑定
     * （MaterialRuntime）——那两层分别由解析流程与**材质运行期共享行/每实例 slot**
     * （support/MaterialRuntimeTable.h）承担。
     *
     * 解析出口只有一个：`BuildResolvedRecipe`。asset 里的默认配方是**基底**，
     * 本组件的配方覆盖是**覆盖源**；参数按声明逐条校验后产出规范化结果。
     * 本步（A2）由调用方把 asset 默认配方作为参数传入；A5 拆分 GeometryData 后
     * 改由数据层自己持引用。
     *
     * 2026-11 收敛：本组件是**薄代理** —— 不含任何数据字段；全部授权状态
     * （配方覆盖 / 具名纹理 / 内容行绑定 / 授权代数）住在世界级的
     * `MaterialAuthoringSideTable`（键 = `EntityID`）。原先的「设备级组件行
     * （`GlobalSSBOType::MaterialData` 池 + `ssbo::MaterialDataRow`）」已整体退役：
     * GPU 从未消费该池，组件行池容量（1024）反而给每实体场景造成假预算。
     */
    class MaterialData : public ComponentRow
    {
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

    private:

        // 2026-11 收敛：**组件不再持任何字段**。授权状态（配方覆盖 / 具名纹理 / 内容行绑定 /
        // 授权代数）的唯一真源是世界级旁表条目（`MaterialAuthoringSideTable`，键 = EntityID）。
        // 原先的 `material_row_id`（设备级组件行行号）与 `authored_generation`（组件副本计数器）
        // 都已并入旁表条目 —— 不存在「组件副本 vs 行 vs 旁表」多套真值。

    public:

        MaterialData() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；对象住**类型化对象池**，析构由池显式调用
        ~MaterialData() = default;

    public:

        // ── 配方覆盖 ──
        void SetRecipe(const hgl::graph::mtl::MaterialRecipe &recipe);
        const hgl::graph::mtl::MaterialRecipe *GetRecipeOverride() const;
        bool HasRecipeOverride() const { return GetRecipeOverride() != nullptr; }

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

        /// 必需纹理的**就绪结论**（三值互斥；见 `EvaluateRequiredTextureReadiness(entry)`）。
        /// 规则本体住在旁表头文件里（无设备依赖）⇒ 可被单元测试直接钉住。
        RequiredTextureReadiness EvaluateRequiredTextureReadiness() const;

        // ── 材质数据行资源（内容行三元组）──
        /// 授权 / 清空本实体引用的**内容行**（PBR/自发光/透射…池里的行）。
        /// `resource.ssbo_id == 0` ⇒ 清空；其余情况要求 binding 有效（无效即报错拒绝）。
        /// 真源写在旁表条目（`Entry::data_resource`）；每次变更推进授权代数。
        bool SetDataResource(const MaterialDataAuthoringResource &resource);

        /// 是否已绑上材质数据行（条目不存在或未授权 ⇒ false）。
        bool HasDataResource() const;

        /// Write a typed material row through a caller-owned lease and bind it.
        /// The accessor must outlive this component's use of the bound row.
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

        /// 授权代数（每次授权状态变化 +1；真源 = 旁表条目 `Entry::authored_generation`）。
        /// 每实例 slot 的跟踪副本（`MaterialRuntimeSlot::tracked_material_data_generation`）
        /// 与之比对，相等即跳过整条解析链。无条目 ⇒ 0。
        /// （2026-11：原 `GetAuthoredRowRevision()` 读设备组件行的行内 revision —— 组件行池
        /// 退役后该名字失去所指，代数读数回归 `GetAuthoredGeneration()`。）
        uint32_t GetAuthoredGeneration() const;

        /// 上游来源（asset 默认配方 / 变体切换）变化时前进授权代数：
        /// 只动代数，不改旁表里的授权数据——下游据此让解析结果失效。
        /// 同时记一次 `TopologyChange::MaterialBinding`（P1：材质批键可能变）。
        void BumpAuthoredGeneration();

        // ── 解析（本层唯一出口）──
        /// @param asset_default_recipe asset 里的默认配方（基底；可为 nullptr）
        bool BuildResolvedRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                 const hgl::graph::ShaderProgram *material_program,
                                 const hgl::graph::mtl::MaterialRecipe *asset_default_recipe) const;

    public:

        void OnDetach();          ///< 卸载钩子（非虚：由类型化池在析构前调用）

    private:

        /// 取本组件所在世界的材质授权旁表（无 owner / 无世界 ⇒ nullptr）
        hgl::ecs::MaterialAuthoringSideTable *GetAuthoringTable() const;
        /// 取（必要时建）本实体的旁表条目；旁表不可用 ⇒ nullptr（已报错）
        MaterialAuthoringSideTable::Entry *GetOrCreateAuthoringEntry();
        /// 查本实体的旁表条目（无则 nullptr；const 视角）
        const MaterialAuthoringSideTable::Entry *FindAuthoringEntry() const;
        /// 授权变更**唯一入口**：条目代数 +1 并记一次 TopologyChange::MaterialBinding。
        /// 所有授权入口（SetRecipe / SetTextureResource / SetDataResource）都必须走它。
        void TouchAuthored();
    };

    /// 槽位映射：`MaterialData` = 材质**数据层**（资源级、跨世界共享）
    template<> struct ComponentTypeOf<MaterialData> { static constexpr ComponentType value = ComponentType::MaterialData; };
}//namespace hgl::ecs
