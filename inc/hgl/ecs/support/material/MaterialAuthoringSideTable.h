#pragma once

// MaterialAuthoringSideTable.h — 材质数据的「CPU 侧授权副本」存储（B4-iii-b 首刀；2026-11 改键）
//
// 为什么要有这张表：`ecs::MaterialData` 是**每实体**的授权状态（配方覆盖 + 具名纹理覆盖 +
// 内容行绑定引用），里面装着 `MaterialRecipe`（含 `AnsiString`/`U8String`/`hgl::ArrayList`）与
// `hgl::UnorderedMap` —— 全都不是 POD，不该进设备侧行 arena。
// ⇒ 组件本体照常活在 ECS 组件池里，它的**授权内容**寄存在这张表：
//   **键 = `EntityID`**（2026-11 起直接以实体 id 为键 —— 原先经 `GlobalSSBOType::MaterialData`
//   组件行池 row_id 转手的间接层已整体退役：GPU 从未消费该池，收口只读旁表），
//   由 `hgl::ecs::Context` 持有、与 `MaterialData::OnDetach` 联动删除。

#include<hgl/CoreType.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>       // hgl::graph::GlobalSSBOBinding（内容行三元组）
#include<hgl/ecs/core/entity/EntityHandle.h>     // hgl::ecs::EntityID（键类型）
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
#include<cstdint>
#include<string>

namespace hgl::ecs
{
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

    /**
     * 材质授权**旁表**（B4-iii-b，2026-11 改键收敛）。
     *
     * 为什么存在：配方覆盖（`mtl::MaterialRecipe`）、具名纹理资源（`UnorderedMap<AnsiString, …>`）
     * 是重类型 ⇒ 不能进任何设备侧行 arena；它们本来就只被 CPU 侧的收集路径消费。
     * **键 = `EntityID`**（原「键 = MaterialData 组件行行号」的间接层已随组件行池退役）。
     *
     * 归属：**世界级**（`ECSContext` 持有，与 MaterialVariantTable / MaterialRuntimeTable 同域）——
     * 授权数据只被本世界的组件与渲染路径消费。
     *
     * 生命周期：**随组件分离而摘除**（`MaterialData::OnDetach`）。
     */
    class MaterialAuthoringSideTable
    {
    public:

        /// 一个实体的材质重类型授权数据（键 = EntityID）
        struct Entry
        {
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
        };

    public:

        /// 取（无则建）实体对应的条目；非法 EntityID ⇒ nullptr
        Entry *GetOrCreate(EntityID entity_id);

        /// 查条目（无则 nullptr）
        Entry *Find(EntityID entity_id);
        const Entry *Find(EntityID entity_id) const;

        /// 摘除条目（组件分离时调用）；返回是否真的摘到
        bool Remove(EntityID entity_id);

    private:

        hgl::UnorderedMap<EntityID, Entry> entries;
    };

    /// 必需纹理的**就绪结论**（三值互斥）。唯一真源：句柄 `PayloadState`。
    enum class RequiredTextureReadiness : uint8_t
    {
        Ready = 0,      ///< 全部就绪（或没有必需纹理）
        Pending,        ///< 至少一张仍在读盘/上传 ⇒ 图元暂不渲染
        Degraded,       ///< 无在途任务，但至少一张已永久失败 ⇒ 句柄 0 占位保底渲染
    };

    /**
     * @brief 由旁表条目算出「必需纹理」的就绪结论。
     *
     * 单独成函数（不吃设备、不吃行号）是为了让渲染门禁的判定规则可被单元测试直接钉住
     * —— 这条规则一旦写错，坏一个贴图就能让整个图元永久消失，且现场只表现为「东西不见了」。
     *
     * @param entry 旁表条目（`nullptr` ⇒ 没有任何授权资源 ⇒ Ready）
     */
    inline RequiredTextureReadiness EvaluateRequiredTextureReadiness(
        const MaterialAuthoringSideTable::Entry *entry)
    {
        if (!entry)
            return RequiredTextureReadiness::Ready;

        // 单趟同时算出两种结论（互斥，优先报「仍在途」）：
        //   · 有一张既没就绪、也没定性失败 ⇒ 等它，本帧不渲染；
        //   · 全部有了结论，但至少一张终态失败 ⇒ 降级：句柄 0 占位保底渲染。
        bool failed = false;

        for (const auto &[named_key, resource] : entry->named_texture_resources)
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

    inline MaterialAuthoringSideTable::Entry *
        MaterialAuthoringSideTable::GetOrCreate(const EntityID entity_id)
    {
        if (!entity_id.IsValid())
        {
            GLogError("[MaterialAuthoringSideTable] 拒绝非法 EntityID（index=0x%08X）",
                      entity_id.index);
            return nullptr;
        }

        if (Entry *exist = entries.GetValuePointer(entity_id))
            return exist;

        if (!entries.Add(entity_id, Entry{}))
        {
            GLogError("[MaterialAuthoringSideTable] 旁表分配失败 entity=%u（内存不足？）",
                      entity_id.index);
            return nullptr;
        }

        return entries.GetValuePointer(entity_id);
    }

    inline MaterialAuthoringSideTable::Entry *
        MaterialAuthoringSideTable::Find(const EntityID entity_id)
    {
        return entity_id.IsValid() ? entries.GetValuePointer(entity_id) : nullptr;
    }

    inline const MaterialAuthoringSideTable::Entry *
        MaterialAuthoringSideTable::Find(const EntityID entity_id) const
    {
        return entity_id.IsValid() ? entries.GetValuePointer(entity_id) : nullptr;
    }

    inline bool MaterialAuthoringSideTable::Remove(const EntityID entity_id)
    {
        return entity_id.IsValid() ? entries.DeleteByKey(entity_id) : false;
    }
}//namespace hgl::ecs
