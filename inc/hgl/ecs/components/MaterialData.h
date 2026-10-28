#pragma once

#include<hgl/ecs/core/ComponentRow.h>
#include<hgl/ecs/support/MaterialAuthoringSideTable.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
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

        // B4-iii-b：**组件不再持任何重类型**——配方覆盖 / 具名纹理资源都落在世界级旁表
        // （`MaterialAuthoringSideTable`，键 = 本组件的材质行行号）。行内只有 POD
        //（授权位 + revision），组件只持行号。⇒ 不存在「组件副本 vs 旁表」双真值。
        //
        // B4-iii-a：材质数据在**设备级**的家的行号（`GlobalSSBOType::MaterialData` 池）。
        // 0 = 尚未分配 / 哨兵（= 旧的「无授权数据行」）；行的内容见 graph::ssbo::MaterialDataRow。
        // 归属设备级、跨世界共享：材质数据是资源级对象，行号必须全局可解析（B4 拍板「路 B」）。
        uint32_t material_row_id = 0;

        // 单调计数器：每次授权状态变化 +1（纹理 / 数据行 / 配方 / 上游来源）。
        // 每实例 slot 的跟踪副本（MaterialRuntimeSlot::tracked_material_data_generation）
        // 与之比对，相等即跳过整条解析链。
        uint32_t authored_generation = 0;

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
        bool SetTextureResource(const std::string &name,
                                hgl::graph::Texture *texture,
                                hgl::graph::Sampler *sampler,
                                MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D,
                                uint32_t array_layer = 0,
                                bool required = false);

        /// 查具名纹理资源。返回条目只要求「表达了某个纹理」（已就绪或只有文件名）；
        /// 是否可绑定由调用方看 `resource.GetTexture()`/`IsReady()`。
        const MaterialTextureAuthoringResource *GetTextureResource(const std::string &name) const;

        // ── 材质数据行资源 ──
        /// 授权 / 清空材质数据行（真源写在 (Global, MaterialData) 组件行里；本组件只持行号）。
        /// `resource.ssbo_id == 0` ⇒ 清空（行保留）；其余情况要求 binding 有效（无效即报错拒绝）。
        void SetDataResource(const MaterialDataAuthoringResource &resource);

        /// 组件侧授权代数（每次授权状态变化 +1）。**行内 revision 与之同源**（见
        /// `GetAuthoredRowRevision`）——只读行的消费者应当读行，不要读组件副本。
        // 2026-10-10（审计 §1.1）：`GetAuthoredGeneration()`（零调用者；B4-ii 起消费者只读行内
        // revision —— 见 Test 35 的禁复活 needle）已删除。成员 `authored_generation` 仍在内部使用。

        /// B4-ii：设备级组件行里的**行内 revision**（= 上面的代数在行内的镜像）。
        /// 消费者（每实例 slot 的变更检测）用这个 ⇒ 状态真源是**行**，不是组件副本。
        /// 无行（组件从未授权 ⇒ 行号 0）⇒ 退回组件计数器（两者此时同为 0，不存在双真值）。
        uint32_t GetAuthoredRowRevision() const;

        /// 上游来源（asset 默认配方 / 变体切换）变化时前进授权代数：
        /// 只动代数，不改本组件持有的授权数据——下游据此让解析结果失效。
        void BumpAuthoredGeneration() { ++authored_generation; }

        // ── 解析（本层唯一出口）──
        /// @param asset_default_recipe asset 里的默认配方（基底；可为 nullptr）
        bool BuildResolvedRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                 const hgl::graph::ShaderProgram *material_program,
                                 const hgl::graph::mtl::MaterialRecipe *asset_default_recipe) const;

    public:

        void OnDetach();          ///< 卸载钩子（非虚：由类型化池在析构前调用）

    private:

        /// 取本组件所在世界的设备级注册表（无 owner / 无世界 / 无设备 ⇒ nullptr）
        hgl::graph::GlobalSSBOBufferRegistry *GetRegistry() const;
        /// B4-iii-b：取本组件所在世界的材质授权旁表（无 owner / 无世界 ⇒ nullptr）
        hgl::ecs::MaterialAuthoringSideTable *GetAuthoringTable() const;
        /// B4-iii-b：取（必要时建）本组件行对应的旁表条目；行不可用 ⇒ nullptr（已报错）
        MaterialAuthoringSideTable::Entry *GetOrCreateAuthoringEntry();
        /// 按需分配组件行（首次授权时）；失败即报错返回 false（不静默）
        bool EnsureMaterialRow();
        /// 读本组件的组件行（未分配返回 nullptr）
        const hgl::graph::ssbo::MaterialDataRow *GetMaterialRow() const;
        /// B4-ii：授权变更**唯一入口**——组件计数器 +1，并（有行时）把
        /// 「(flags 置位 ∧ clear_flags 清位) + 新 revision」写进组件行。
        /// 所有授权入口（SetRecipe / SetTextureResource / SetDataResource）
        /// 都必须走它，不得自己 `++authored_generation`（否则行内 revision 与代数列不同步）。
        void TouchAuthoredRow(uint32_t set_flags, uint32_t clear_flags);
        /// 写本组件的组件行
        bool WriteMaterialRow(const hgl::graph::ssbo::MaterialDataRow &row);
        /// 清空组件行内容（保留行号）
        void ClearMaterialRow();
    };

    /// 槽位映射：`MaterialData` = 材质**数据层**（资源级、跨世界共享）
    template<> struct ComponentTypeOf<MaterialData> { static constexpr ComponentType value = ComponentType::MaterialData; };
}//namespace hgl::ecs
