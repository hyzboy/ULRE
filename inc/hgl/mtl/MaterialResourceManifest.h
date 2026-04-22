#pragma once

#include <hgl/mtl/StaticMaterialDef.h>

namespace hgl::graph::mtl
{
    /// MaterialResourceManifest — 材质资源需求的唯一中间表示。
    ///
    /// 设计原则：
    ///   1. **Owning**：自带 ubos / ssbos / samplers 三个 storage，
    ///      调用方无需再单独管理 storage 生命周期。
    ///   2. **Value semantics**：可拷贝、可移动、可作为函数参数与返回值。
    ///   3. **小巧**：仅包含资源"需求"（哪些 semantic / 哪些 slot），不含
    ///      set/binding 编号；编号由 MaterialDescriptorDB 在 Resort() 时分配。
    ///   4. **来源中立**：可从 StaticMaterialDef（前置式）或
    ///      ShaderResourceScanner 反射结果转换得到。
    class MaterialResourceManifest
    {
    public:
        UBOSemanticSet                  ubos;
        SSBOSemanticSet                 ssbos;
        StaticTextureSamplerDescriptors samplers;

    public:
        // 构造 --------------------------------------------------------
        MaterialResourceManifest() = default;
        MaterialResourceManifest(const MaterialResourceManifest &) = default;
        MaterialResourceManifest(MaterialResourceManifest &&) noexcept = default;
        MaterialResourceManifest &operator=(const MaterialResourceManifest &) = default;
        MaterialResourceManifest &operator=(MaterialResourceManifest &&) noexcept = default;

        // 转换工厂 ----------------------------------------------------

        /// 从 StaticMaterialDef 的 3 个借用指针中拷贝出一份独立 manifest。
        /// 任意指针为 nullptr 时视为空集合。
        static MaterialResourceManifest FromStaticDef(const StaticMaterialDef &def);

        // 操作 --------------------------------------------------------

        /// 把 other 的所有需求并入本 manifest。
        /// sampler slot 冲突时以 other 的值为准（覆盖语义）。
        void MergeOverwrite(const MaterialResourceManifest &other);

        /// 与 MergeOverwrite 相反：sampler slot 冲突时保留本 manifest 已有值。
        /// 等价于原 ShaderResourceScanner.cpp 中 try_emplace 的行为。
        void MergeKeepFirst(const MaterialResourceManifest &other);

        // 便捷接口 ----------------------------------------------------
        bool empty() const { return ubos.empty() && ssbos.empty() && samplers.empty(); }
        void clear() { ubos.clear(); ssbos.clear(); samplers.clear(); }

        // 与旧接口的桥接 ----------------------------------------------

        /// **过渡期**：把 manifest 投影回 StaticMaterialDef 的 3 个借用指针。
        /// 仅用于尚未迁移完成的下游；新的 API 请直接接受
        /// const MaterialResourceManifest &。
        ///
        /// 返回的 def 借出本 manifest 的 storage——因此本对象必须比 def 长寿。
        /// CreateFromFixedDef2D / CreateFromFixedDef3D 是同步调用，在它们
        /// 返回前一定消费完 def，因此在同一函数体内使用是安全的。
        StaticMaterialDef ProjectIntoStaticDef(const StaticMaterialDef &base_def) const;
    };

} // namespace hgl::graph::mtl
