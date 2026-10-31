#pragma once

/**
 * 材质数据块**资产句柄**（`hgl::asset::AssetObject` 派生）。
 *
 * 解决的问题：批量材质示例（`PBRSpheres` 那种 10×10 网格）原本在 C++ 里双循环算出
 * 几百行 SSBO 数据再逐行 `SetMaterialDataRow`，数据只活在代码里 —— 没法 diff、没法
 * 交给美术、也没法跨示例复用。数据块资产把这段数据变成**有身份、可加载、可上传**的
 * 一份东西：
 *
 * | 层 | 类型 | 回答的问题 |
 * |----|------|-----------|
 * | 授权层 | `MaterialDataBlockSpec` | 文件里写了什么键（检查器 / diff / 文档） |
 * | 句柄层 | `MaterialDataBlockAsset`（本类） | 这份数据叫什么、从哪来、多少行、上传没上传 |
 * | 物理层 | `MaterialDataBlock` | 与 GPU 行逐字节同构的 payload |
 * | GPU 层 | `GlobalSSBORowRangeLease` | 落在哪个池、首行号、整段行号连续 |
 *
 * ── 两种来源，一条路径 ────────────────────────────────────────────────────
 *
 * · TOML（`.material_data.toml`）：`ParseMaterialDataBlockFile` → 规约 + 摊平 payload，
 *   **有**授权形态（`HasSpec()` 为真），检查器可以回写；
 * · `MDB1` 二进制：只校验 16 字节头 + 拷一份 payload，**没有**授权形态 —— 它就是
 *   「程序生成好的一坨字节」，不需要也不应该假装自己有字段语义。
 *
 * 两者的上传完全同路（`UploadMaterialDataBlock` 收 `MaterialDataBlockView`）。
 *
 * ── 状态只有三态 ──────────────────────────────────────────────────────────
 *
 * ```
 * Parsed ──Upload 成功──> Ready
 *    └──解析失败────────> Failed
 * ```
 *
 * 上传失败（没绑池 / 行宽不符 / 池满）**不是** `Failed`：数据本身没问题，只是还没
 * 上船。这类句柄停在 `Parsed` 并给出 `GetFailureReason()`，工具仍然能读它的行。
 * 这一点与 `MaterialRecipeAsset` 的纹理就地等待同构。
 *
 * 生命周期：句柄由 `MaterialDataBlockRegistry` 的表拥有并销毁；上传成功后本类持有
 * 行区间租约（析构即归还整段）。线程约定：主线程。
 */

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/mtl/MaterialDataBlock.h>
#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    /// 数据块资产的装配阶段（见文件头「状态只有三态」）
    enum class MaterialDataBlockAssetState : uint8
    {
        Parsed = 0,     ///<数据已就绪（含已上传但未成功的「等池」情形按 `IsUploaded()` 区分）
        Ready,          ///<已落入 GPU 池，行区间可用
        Failed,         ///<解析失败，`GetFailureReason()` 给出原因

        ENUM_CLASS_RANGE(Parsed, Failed)
    };

    const char *GetMaterialDataBlockAssetStateName(MaterialDataBlockAssetState state) noexcept;

    /**
     * 一份材质数据块的资产句柄。
     *
     * 不可拷贝/移动：区间租约的归还责任与句柄身份必须一一对应，复制会让两段代码
     * 以为自己拥有同一千行。
     */
    class MaterialDataBlockAsset final : public hgl::asset::AssetObject
    {
    private:

        MaterialDataBlockSpec       spec;                   ///<授权形态（`MDB1` 来源时为空且 `has_spec == false`）
        bool                        has_spec = false;
        MaterialDataBlock           block;                  ///<物理行 payload（自有）
        MaterialDataBlockParseError parse_error;            ///<解析失败时的定位信息

        OSString                    source_path;            ///<来源文件（可空 = 内存注册）
        U8String                    resource_id;            ///<资产身份（`mtldata:@<name>` / `mtldata:file:<path>`）
        AnsiString                  asset_name;             ///<`AssetObject` 契约要求的稳定名字

        AnsiString                  failure_reason;         ///<`Failed` 或上传失败的原因（诊断文案）
        MaterialDataBlockAssetState state = MaterialDataBlockAssetState::Parsed;

        GlobalSSBORowRangeLease     row_lease;              ///<上传成功后持有的整段行租约

    public:

        /**
         * @param in_data       解析结果（payload 被移入，`parse_error` 保留诊断）
         * @param in_spec       授权形态；`has_spec == false` 时被忽略
         * @param has_spec      是否存在授权形态（`MDB1` 来源为 false）
         */
        MaterialDataBlockAsset(MaterialDataBlockFileData &&in_data,
                               MaterialDataBlockSpec      &&in_spec,
                               const bool                   has_spec);

        ~MaterialDataBlockAsset() override = default;

        NO_COPY(MaterialDataBlockAsset)
        NO_MOVE(MaterialDataBlockAsset)

    public:     // AssetObject 契约

        /// 反射描述取**授权规约**表；`MDB1` 来源没有字段语义，仍返回同一张表
        /// （检查器据此知道「这类资产本该长什么样」），只是 `HasSpec()` 为假。
        const hgl::asset::ClassMetaInfo *GetClassMetaInfo() const override;

        const AnsiString &GetAssetName() const override { return asset_name; }

    public:     // 只读观察

        const MaterialDataBlockSpec &GetSpec() const { return spec; }
        const MaterialDataBlock &GetBlock() const { return block; }

        /// 是否带授权形态（TOML 来源为真，`MDB1` 来源为假）
        bool HasSpec() const { return has_spec; }

        const MaterialDataBlockParseError &GetParseError() const { return parse_error; }

        const OSString &GetSourcePath() const { return source_path; }

        /// 资产身份：有名走 `mtldata:@<name>`，无名走 `mtldata:file:<path>`
        const U8String &GetResourceId() const { return resource_id; }

        MaterialDataBlockAssetState GetState() const { return state; }

        bool IsReady() const { return state == MaterialDataBlockAssetState::Ready; }

        bool IsFailed() const { return state == MaterialDataBlockAssetState::Failed; }

        bool IsUploaded() const { return row_lease.IsValid(); }

        const AnsiString &GetFailureReason() const { return failure_reason; }

    public:     // 行信息（不论是否上传都能回答）

        GlobalSSBOType GetGlobalSSBOType() const { return block.type; }

        uint32 GetRowBytes() const { return block.row_bytes; }

        uint32 GetRowCount() const { return block.GetRowCount(); }

        uint32 GetByteSize() const { return block.GetByteSize(); }

    public:     // GPU 侧（仅在 `IsUploaded()` 为真时有意义）

        uint32 GetFirstRow() const { return row_lease.GetFirstRow(); }

        uint32 GetLastRow() const { return row_lease.GetLastRow(); }

        uint32 GetSSBOId() const { return row_lease.GetSSBOId(); }

        /// 块内第 index 行对应的池行号（越界 / 未上传返回 `InvalidRowID`）
        uint32 RowIDAt(uint32 index) const { return row_lease.RowIDAt(index); }

        const GlobalSSBORowRangeLease &GetRowLease() const { return row_lease; }

    public:     // 装配

        /// 记录来源与身份（`label` 为路径或空）
        void SetSource(const os_char *label);

        /// 校验失败 / 解析失败：⇒ `Failed`
        void MarkFailed(const char *reason);

        /**
         * 落到全局池上：申请整段连续行 → 整段拷贝 → 整段标脏，成功后持有租约。
         *
         * @param registry 全局 SSBO 注册表；**为空即失败**（未绑图形设备时的正常路径），
         *                 句柄停在 `Parsed`，不标 `Failed`
         * @return 已上传（含重复调用）为 `true`
         */
        bool Upload(GlobalSSBOBufferRegistry *registry);
    };//class MaterialDataBlockAsset
}//namespace hgl::graph::mtl

