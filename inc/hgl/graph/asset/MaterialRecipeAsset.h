#pragma once

/**
 * 材质配方**资产句柄**（`hgl::asset::AssetObject` 派生）。
 *
 * 为什么需要：`MaterialRecipe`（`inc/hgl/mtl/MaterialRecipe.h`）是**运行时结果** ——
 * 它带着纹理句柄、已解析的 `VertexShaderNodeConfig` 与归一化后的渲染状态，既算不出
 * 稳定哈希，也没法回答「文件里写了什么」。而 `MaterialRecipeSpec` 反过来只有作者侧
 * 字段，回答不了「能不能画」。两者之间缺一个**有身份的句柄**：
 *
 * | 层 | 类型 | 回答的问题 |
 * |----|------|-----------|
 * | 规约层 | `MaterialRecipeSpec` | 文件里写了什么键（检查器 / diff / 文档） |
 * | 句柄层 | `MaterialRecipeAsset`（本类） | 这份配方叫什么、从哪来、校验过没有、纹理就绪没 |
 * | 运行时层 | `MaterialRecipe` | 渲染侧真正消费的已解析结果 |
 *
 * 本类同时持规约与运行时两份数据，因此外部工具可以只靠一个指针拿到全部三个视角，
 * 不必自己再把「名 → 规约」「名 → recipe」两份表配对。
 *
 * ── 为什么状态机只有三态 ──────────────────────────────────────────────────
 *
 * 配方本体是**纯文本解析**（毫秒级、无 IO 等待），没有可挂起的加载阶段；真正异步的
 * 是它引用的纹理。因此这里**不**复刻 `PayloadState` 那套六态机，只区分：
 *
 * ```
 * Parsed ──校验通过──> Ready
 *    └────校验失败────> Failed
 * ```
 *
 * 异步轴留给纹理句柄自己（`TextureAsset::GetPayloadState()`），本类用
 * `AreTexturesResident()` / `GetPendingTextureCount()` 把它**聚合**成一个可门禁的
 * 就绪度 —— 这正是未来统一流式加载系统需要的那一个查询，而它现在就有真实语义，
 * 不是占位桩。
 *
 * 生命周期：句柄由 `MaterialRecipeRegistry` 的表拥有并销毁，配方只持裸观察指针。
 * 线程约定：只在主线程使用（校验阶段会创建纹理句柄）。
 */

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    /// 配方资产的装配阶段（见文件头「为什么状态机只有三态」）
    enum class MaterialRecipeAssetState : uint8
    {
        Parsed = 0,     ///<字段已解析，领域契约与纹理槽**尚未**校验
        Ready,          ///<校验通过，可参与渲染
        Failed,         ///<校验失败，`GetFailureReason()` 给出原因

        ENUM_CLASS_RANGE(Parsed, Failed)
    };

    const char *GetMaterialRecipeAssetStateName(MaterialRecipeAssetState state) noexcept;

    /**
     * 一份材质配方的资产句柄。
     *
     * 不可拷贝：句柄身份（`resource_id`）必须一一对应唯一的表项。
     */
    class MaterialRecipeAsset final : public hgl::asset::AssetObject
    {
    private:

        MaterialRecipeSpec              spec;           ///<作者写了什么（检查器 / 文档视角）
        MaterialRecipe                  recipe;         ///<已解析的运行时结果（渲染视角）
        MaterialRecipeFileParseError    parse_error;    ///<解析失败时的定位信息（成功时为空）

        OSString                        source_path;    ///<来源 `.recipe.toml`（可空 = 内存注册）
        U8String                        resource_id;    ///<资产身份（`rcp:@<name>`；无名兜底 `rcpfile:<path>`）
        AnsiString                      asset_name;     ///<`AssetObject` 契约要求的稳定名字

        AnsiString                      failure_reason; ///<`Failed` 态的原因（诊断文案）
        MaterialRecipeAssetState        state = MaterialRecipeAssetState::Parsed;

    public:

        /// 直接以「已解析结果 + 规约」建句柄（注册器解析完立刻构造，避免二次解析）
        MaterialRecipeAsset(const MaterialRecipeFileData &data,
                            MaterialRecipeSpec            &&parsed_spec);

        ~MaterialRecipeAsset() override = default;

        NO_COPY(MaterialRecipeAsset)
        NO_MOVE(MaterialRecipeAsset)

    public:     // AssetObject 契约

        /**
         * 反射描述取**规约**表（`Class_MaterialRecipeSpec`）。
         *
         * 检查器要编辑的是 `*.recipe.toml` 里的键，不是运行时的纹理句柄与 SSBO 绑定，
         * 因此这里必须给规约而非 `MaterialRecipe`：后者没有生成元数据，也无法往返。
         */
        const hgl::asset::ClassMetaInfo *GetClassMetaInfo() const override;

        const AnsiString &GetAssetName() const override { return asset_name; }

    public:     // 只读观察

        const MaterialRecipeSpec &GetSpec() const { return spec; }
        const MaterialRecipe &GetRecipe() const { return recipe; }

        const MaterialRecipeFileParseError &GetParseError() const { return parse_error; }

        const OSString &GetSourcePath() const { return source_path; }

        /// 资产身份：配方名唯一 ⇒ `rcp:@<name>`；无名（解析器已拒绝）兜底 `rcpfile:<path>`
        const U8String &GetResourceId() const { return resource_id; }

        MaterialRecipeAssetState GetState() const { return state; }

        bool IsReady() const { return state == MaterialRecipeAssetState::Ready; }

        const AnsiString &GetFailureReason() const { return failure_reason; }

    public:     // 装配（只由 MaterialRecipeRegistry 调用）

        /**
         * 校验阶段的**可写视图**。
         *
         * 存在的唯一理由：校验要往 `recipe` 里填纹理句柄（`AddTexture`），那是这份
         * 数据一生中唯一一次合法写入。除此之外一律走 `GetRecipe()` 只读。
         */
        MaterialRecipe &EditRecipe() { return recipe; }

        /// 记录来源与身份（`label` 为路径或 `<memory>`）
        void SetSource(const os_char *label);

        /// 校验通过：`Parsed` ⇒ `Ready`
        void MarkReady() noexcept;

        /// 校验失败：`Parsed` ⇒ `Failed`；`reason` 只做诊断，不参与渲染
        void MarkFailed(const char *reason);

    public:     // 聚合就绪度（异步轴在纹理句柄上，这里只做汇总）

        /**
         * 所有**已声明**的纹理句柄是否都已驻留。
         *
         * 未绑定句柄的槽（`asset == nullptr`）按 `required` 判定：必需 ⇒ 未就绪，
         * 可选 ⇒ 视作就绪。无纹理的配方恒为 `true`。
         */
        bool AreTexturesResident() const;

        /// 尚未驻留的纹理槽数量（0 ⇒ `AreTexturesResident()` 为真）
        int GetPendingTextureCount() const;
    };//class MaterialRecipeAsset
}//namespace hgl::graph::mtl
