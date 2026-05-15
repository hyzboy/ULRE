#ifndef HGL_MTL_MATERIAL_VARIANT_REGISTRY_H
#define HGL_MTL_MATERIAL_VARIANT_REGISTRY_H

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <functional>
#include <string>
#include <vector>
#include <ankerl/unordered_dense.h>

namespace hgl::graph::mtl
{
    /// RegistryLookupOptions - 注册表查询选项
    struct RegistryLookupOptions
    {
        /// 若为 true，则将 effective_feature_mask 纳入查询 key；
        /// 默认 false，保持与注册时行为一致。
        bool match_effective_feature_mask = false;

        /// 当同一 key 存在多个 factory_type 候选时，优先匹配该 preset。
        std::optional<MaterialPreset> preferred_factory_type;
    };

    /// VariantRegistry - 变体注册表
    /// 维护从 MaterialVariantKey 到 MaterialVariantDesc 的映射
    class VariantRegistry
    {
    public:
        VariantRegistry() = default;
        ~VariantRegistry() = default;

        // 注册一个新的材质变体
        void RegisterVariant(const MaterialVariantKey &key, const MaterialVariantDesc &desc);

        // 查询变体描述
        const MaterialVariantDesc *QueryVariant(const MaterialVariantKey &key,
                                                const RegistryLookupOptions &options = {}) const;

        // 查询变体描述（带 canonical fallback）。
        // fallback 规则：
        //   step0: canonicalize sky/light 到 Simple/Lambert
        const MaterialVariantDesc *QueryVariantWithCanonicalFallback(
            const MaterialVariantKey &key,
            MaterialVariantKey *resolved_key = nullptr,
            const RegistryLookupOptions &options = {}) const;

        // 校验内置变体模板是否可组装（文件存在性 + 路由有效性）
        bool ValidateBuiltinVariantTemplates(const std::string &shader_library_path,
                                             std::vector<std::string> &diagnostics) const;

        // 导出稳定顺序的快照文本，便于回归比对。
        std::string DumpSnapshot() const;

        // 导出 Phase 2 builtin row 快照文本，便于检查表驱动字段。
        std::string DumpBuiltinRowSnapshot() const;

        // 导出 Phase 2 资源政策审计文本，记录每个 row 的显式资源需求配置。
        std::string DumpResourcePolicyAudit() const;

        // 初始化内置变体
        void InitializeBuiltinVariants();

        // 遍历所有注册变体
        void ForEach(std::function<void(const MaterialVariantKey &, const MaterialVariantDesc &)> cb) const;

        // 遍历所有内置 row（Phase 2 table model）
        void ForEachBuiltinRow(std::function<void(const MaterialVariantRow &)> cb) const;

        // 返回已注册变体数量
        size_t Size() const noexcept { return variant_count; }

    private:
        struct VariantEntry
        {
            MaterialVariantKey key;
            MaterialVariantDesc desc;
        };

        // hash bucket -> candidates (supports same-key multi-factory variants)
        ankerl::unordered_dense::map<uint64, std::vector<VariantEntry>> variant_map;
        std::vector<MaterialVariantRow> builtin_row_storage;
        size_t variant_count = 0;
    };

    /// 返回全局内置变体注册表单例（首次调用时初始化）
    const VariantRegistry &GetBuiltinVariantRegistry();

    // ---------------------------------------------------------------------------
    // VariantRegistryStatsSink — 可注入的统计回调接口
    // ---------------------------------------------------------------------------

    class VariantRegistryStatsSink
    {
    public:
        virtual ~VariantRegistryStatsSink() = default;
        virtual void OnExactMatch(const MaterialVariantKey &request_key, const MaterialVariantDesc &desc) = 0;
        virtual void OnMiss(const MaterialVariantKey &request_key) = 0;
    };

    /// 设置全局统计 sink（nullptr 表示禁用）。线程安全。
    void SetGlobalVariantRegistryStatsSink(VariantRegistryStatsSink *sink);

    /// 获取当前全局统计 sink。线程安全。
    VariantRegistryStatsSink *GetGlobalVariantRegistryStatsSink();
}

#endif // HGL_MTL_MATERIAL_VARIANT_REGISTRY_H
