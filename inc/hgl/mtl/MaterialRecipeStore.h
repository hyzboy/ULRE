#pragma once

/// MaterialRecipeStore.h — 材质配方 ID 注册表
///
/// 职责：将 MaterialRecipe 对象分配稳定的 32 位整数 ID，并以内容哈希做去重。
/// 与 MaterialRecipeRegistry（运行时 Vulkan 工厂）完全无关；
/// 此类不持有任何 Vulkan 对象，不依赖 GPU 资源，可在 ECS 层安全持有 ID 句柄。
///
/// 线程安全：RegisterRecipe / UpdateRecipe 持写锁，其余接口持读锁。

#include <hgl/mtl/MaterialRecipeID.h>
#include <hgl/mtl/MaterialRecipe.h>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace hgl::graph::mtl
{

    /// 材质配方存储注册表
    class MaterialRecipeStore
    {
    public:
        MaterialRecipeStore()  = default;
        ~MaterialRecipeStore() = default;

        MaterialRecipeStore(const MaterialRecipeStore &) = delete;
        MaterialRecipeStore &operator=(const MaterialRecipeStore &) = delete;

        // ─────────────────────────────────────────────────────────────────────
        // 写操作
        // ─────────────────────────────────────────────────────────────────────

        /// 注册一个配方，返回分配的 ID。
        /// 若已存在内容完全一致的配方，直接返回其已有 ID（内容去重）。
        MaterialRecipeID RegisterRecipe(const MaterialRecipe &recipe);

        /// 用新配方替换已有 ID 对应的内容。
        /// 如果 id 无效，返回 false。
        /// 注意：替换后旧哈希映射被移除，新哈希映射被建立；
        ///       原来指向该 ID 的使用方会在下次 NeedsResolve 时重新解析。
        bool UpdateRecipe(MaterialRecipeID id, const MaterialRecipe &new_recipe);

        // ─────────────────────────────────────────────────────────────────────
        // 读操作
        // ─────────────────────────────────────────────────────────────────────

        /// 通过 ID 获取配方指针；ID 无效时返回 nullptr。
        const MaterialRecipe *GetRecipe(MaterialRecipeID id) const;

        /// 通过内容哈希查找 ID；未找到时返回 kInvalidMaterialRecipeID。
        MaterialRecipeID FindByContentHash(uint64_t hash) const;

        /// 计算配方的内容哈希（公开以便测试 / 调试）。
        static uint64_t ComputeContentHash(const MaterialRecipe &recipe);

        /// 当前注册的配方总数。
        size_t Size() const;

    private:
        mutable std::shared_mutex                    mutex_;
        /// 配方存储；下标 = (id - 1)，从 1 开始分配 ID。
        std::vector<MaterialRecipe>                  recipes_;
        /// 内容哈希 → ID 映射（去重用）。
        std::unordered_map<uint64_t, MaterialRecipeID> hash_map_;
    };

} // namespace hgl::graph::mtl
