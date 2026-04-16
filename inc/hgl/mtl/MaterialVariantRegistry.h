#ifndef HGL_MTL_MATERIAL_VARIANT_REGISTRY_H
#define HGL_MTL_MATERIAL_VARIANT_REGISTRY_H

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <string>
#include <vector>
#include <ankerl/unordered_dense.h>

namespace hgl::graph::mtl
{
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
        const MaterialVariantDesc *QueryVariant(const MaterialVariantKey &key) const;

        // 查询变体描述（带 canonical fallback）。
        // fallback 规则：清除 per-slot/source feature 位后重试。
        const MaterialVariantDesc *QueryVariantWithCanonicalFallback(
            const MaterialVariantKey &key,
            MaterialVariantKey *resolved_key = nullptr) const;

        // 校验内置变体模板是否可组装（文件存在性 + 路由有效性）
        bool ValidateBuiltinVariantTemplates(const std::string &shader_library_path,
                                             std::vector<std::string> &diagnostics) const;

        // 导出稳定顺序的快照文本，便于回归比对。
        std::string DumpSnapshot() const;

        // 根据 preset（兼容层）查询对应的 key
        MaterialVariantKey MapPresetToVariantKey(MaterialPreset preset) const;

        // 初始化内置变体
        void InitializeBuiltinVariants();

    private:
        struct VariantEntry
        {
            MaterialVariantKey key;
            MaterialVariantDesc desc;
        };

        // 使用 key 的哈希作为查询键
        ankerl::unordered_dense::map<uint64, VariantEntry> variant_map;
    };

    /// 返回全局内置变体注册表单例（首次调用时初始化）
    const VariantRegistry &GetBuiltinVariantRegistry();
}

#endif // HGL_MTL_MATERIAL_VARIANT_REGISTRY_H
