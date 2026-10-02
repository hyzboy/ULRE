#pragma once

#include<hgl/ecs/core/Component.h>
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
    class MaterialData : public Component
    {
    public:
        enum class MaterialTextureResourceKind : uint8_t
        {
            Texture2D = 0,
            Texture2DArray
        };

        struct MaterialTextureAuthoringResource
        {
            std::string resource_id;
            hgl::graph::Texture *texture = nullptr;
            hgl::graph::Sampler *sampler = nullptr;
            MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D;
            uint32_t array_layer = 0;
            bool required = false;
        };

        struct MaterialDataAuthoringResource
            : hgl::graph::GlobalSSBOBinding
        {
            // B4-i：GPU 面只剩**类型 + 行号**（基类 `GlobalSSBOBinding`）——原先还挂着每实体的
            // 缓冲指针 + 元素容量 + 字节步长三个**死字段**（实测外部读者 0、示例赋值 0 处；容量与
            // 步长的真源在 `SSBOBufferRegistry::RegisterBuffer`，由字节数 ÷ 容量反算步长）
            // ⇒ 与 B3-iii「删几何三指针」同构，整组删除；名字不再出现，契约按「零残留」判。
            bool authored = false;

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

        // 配方覆盖（authoring）：有则整体覆盖 asset 默认配方。
        bool has_recipe_override = false;
        hgl::graph::mtl::MaterialRecipe recipe_override;

        hgl::UnorderedMap<hgl::AnsiString, MaterialTextureAuthoringResource>
            named_texture_resources;

        // B4-iii-a：材质数据在**设备级**的家的行号（`GlobalSSBOType::MaterialData` 池）。
        // 0 = 尚未分配 / 哨兵（= 旧的「无授权数据行」）；行的内容见 graph::ssbo::MaterialDataRow。
        // 归属设备级、跨世界共享：材质数据是资源级对象，行号必须全局可解析（B4 拍板「路 B」）。
        uint32_t material_row_id = 0;

        // 单调计数器：每次授权状态变化 +1（纹理 / 数据行 / 配方 / 上游来源）。
        // 每实例 slot 的跟踪副本（MaterialRuntimeSlot::tracked_material_data_generation）
        // 与之比对，相等即跳过整条解析链。
        uint32_t authored_generation = 0;

    public:

        explicit MaterialData(const std::string &name = "MaterialData")
            : Component(name)
        {
        }

        ~MaterialData() override = default;

    public:

        // ── 配方覆盖 ──
        void SetRecipe(const hgl::graph::mtl::MaterialRecipe &recipe);
        const hgl::graph::mtl::MaterialRecipe *GetRecipeOverride() const;
        bool HasRecipeOverride() const { return GetRecipeOverride() != nullptr; }

        // ── 具名纹理资源 ──
        bool SetTextureResource(const std::string &name,
                                const MaterialTextureAuthoringResource &resource);
        bool SetTextureResource(const std::string &name,
                                hgl::graph::Texture *texture,
                                hgl::graph::Sampler *sampler,
                                MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D,
                                const std::string &resource_id = std::string(),
                                uint32_t array_layer = 0,
                                bool required = false);
        const MaterialTextureAuthoringResource *GetTextureResource(const std::string &name) const;

        // ── 材质数据行资源 ──
        /// 授权 / 清空材质数据行（真源写在 (Global, MaterialData) 组件行里；本组件只持行号）。
        /// `resource.ssbo_id == 0` ⇒ 清空（行保留）；其余情况要求 binding 有效（无效即报错拒绝）。
        void SetDataResource(const MaterialDataAuthoringResource &resource);

        /// 本组件的设备级组件行行号（0 = 无材质数据）
        uint32_t GetMaterialRowID() const { return material_row_id; }

        /// 清空全部授权资源（纹理 + 数据行；配方覆盖不在其内）
        void ClearAuthoredResources();

        uint32_t GetAuthoredGeneration() const { return authored_generation; }

        /// 上游来源（asset 默认配方 / 变体切换）变化时前进授权代数：
        /// 只动代数，不改本组件持有的授权数据——下游据此让解析结果失效。
        void BumpAuthoredGeneration() { ++authored_generation; }

        // ── 解析（本层唯一出口）──
        /// @param asset_default_recipe asset 里的默认配方（基底；可为 nullptr）
        bool BuildResolvedRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                 const hgl::graph::ShaderProgram *material_program,
                                 const hgl::graph::mtl::MaterialRecipe *asset_default_recipe) const;

    public:

        void OnDetach() override;

    private:

        /// 取本组件所在世界的设备级注册表（无 owner / 无世界 / 无设备 ⇒ nullptr）
        hgl::graph::GlobalSSBOBufferRegistry *GetRegistry() const;
        /// 按需分配组件行（首次授权时）；失败即报错返回 false（不静默）
        bool EnsureMaterialRow();
        /// 读本组件的组件行（未分配返回 nullptr）
        const hgl::graph::ssbo::MaterialDataRow *GetMaterialRow() const;
        /// 写本组件的组件行
        bool WriteMaterialRow(const hgl::graph::ssbo::MaterialDataRow &row);
        /// 清空组件行内容（保留行号）
        void ClearMaterialRow();
    };

    /// 槽位映射：`MaterialData` = 材质**数据层**（资源级、跨世界共享）
    template<> struct ComponentTypeOf<MaterialData> { static constexpr ComponentType value = ComponentType::MaterialData; };
}//namespace hgl::ecs
