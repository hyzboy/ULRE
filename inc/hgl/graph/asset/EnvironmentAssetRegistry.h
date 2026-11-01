#pragma once

/**
 * 环境配置资产表（`env:` URI 的唯一入口）。
 *
 * 为什么要有一张表：环境配置的消费点是 `ecs::EnvironmentSystem::SetupSkyEnvironment()`，
 * 它可能被反复调用（热切换天空、切关卡重复进入）。没有表的话每次都要重新读盘 + 重新
 * 解析 TOML，而且「同一个 `env:@Sky34` 两次调用拿到两份不同的数据」会让缓存判定失效
 * （`SetupSkyEnvironment` 内部对贴图句柄做值比较来决定要不要重建）。
 *
 * URI 语法与其它资产表完全一致（`<type>:<source>:<target>`）：
 *
 * ```
 * env:file:res/env/sky_34.env.toml   落盘文件（source=File）
 * env:@Sky34                         表内名字（source=Builtin，target 以 '@' 开头）
 * res/env/sky_34.env.toml            省略类型段 ⇒ 走本表的 default_type（文件语义）
 * ```
 *
 * 缓存键是**规范化后的路径**（`AssetURI::Normalize`），因此 `res\a.env.toml`、
 * `./res/a.env.toml`、`env:file:res/a.env.toml` 命中同一份资产。
 *
 * 线程约定：主线程（与所有其它资产表一致，内部无锁）。
 */

#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    /**
     * 环境配置资产表。
     *
     * 解析在这里发生，**句柄只表示「已通过校验的合法环境」**：字段自洽性检查
     * （`schema` 版本、`sh9.bands` 必须是 9 条……）不过就注册失败，原因留给
     * `GetLastError()`。这样下游 `SetupSkyEnvironment(uri)` 不需要再做任何校验，
     * 也不会出现"拿到半个配置"的中间态。
     */
    class EnvironmentAssetRegistry
    {
    private:

        ManagedArray<EnvironmentAsset>  assets;

        AnsiString                      last_error;     ///<最近一次失败的完整原因

    private:

        /// 字段自洽性检查（跨字段业务规则，元数据层不表达）
        bool Validate(const EnvironmentSpec &spec, AnsiString &out_error) const;

        EnvironmentAsset *Attach(EnvironmentSpec &&spec, const os_char *label);

    public:

        EnvironmentAssetRegistry() = default;
        ~EnvironmentAssetRegistry() = default;

        NO_COPY(EnvironmentAssetRegistry)
        NO_MOVE(EnvironmentAssetRegistry)

    public:     // 装配

        /**
         * 从内存里的 TOML 文本注册一份环境配置。
         *
         * `label` 只用于报错与身份（不会读盘），可以传 `nullptr`。
         * 返回 `nullptr` 表示失败，原因见 `GetLastError()`。
         */
        EnvironmentAsset *RegisterFromToml(const os_char *label, const char *content, const int size);

        /**
         * 从二进制 FlatBuffers 缓冲注册一份环境配置（`.env.fb` / `.env`）。
         *
         * `label` 只用于报错与身份（不会读盘），可以传 `nullptr`。
         * 返回 `nullptr` 表示失败，原因见 `GetLastError()`。
         */
        EnvironmentAsset *RegisterFromFlat(const os_char *label, const uint8 *data, const uint64 size);

        /// 读文件并注册（`.env` / `.env.fb` / `.env.toml`）。返回 `nullptr` 表示失败。
        EnvironmentAsset *LoadFile(const OSString &path);

        /**
         * 扫描目录里所有 `.env.toml` 并注册（不递归）。
         *
         * 存在的理由：`env:@<name>` 只能命中**已注册**的条目，而名字分散在文件里，
         * 没法从 URI 反推路径。想要"按名字引用"就得先有一次目录入口。目录本身
         * 不存在或空目录不算失败（返回 `true`、数量为 0），单文件失败计入
         * `out_error_count` 但不会中断整批。
         */
        bool LoadDirectory(const OSString &directory,
                           int *out_file_count = nullptr,
                           int *out_error_count = nullptr);

    public:     // 查询

        /**
         * URI 门面：解析 → 类型校验 → 名字/路径 → 缓存 → 按需读盘。
         *
         * 类型段不是 `env` 时**明确拒绝**（而不是当路径去读盘）——静默当路径处理会让
         * 「把 `geo:file:x` 传给环境槽」这种拼写错误变成一次莫名其妙的读盘失败。
         */
        EnvironmentAsset *GetOrLoadURI(const OSString &uri);

        EnvironmentAsset *FindByName(const char *name) const;

        EnvironmentAsset *FindAssetByPath(const OSString &path) const;

        EnvironmentAsset *FindByResourceId(const U8String &resource_id) const;

        EnvironmentAsset *GetAssetAt(const int index) const;

        int GetCount() const { return assets.GetCount(); }

        /// 最近一次失败的原因（诊断用，可能为空）
        const AnsiString &GetLastError() const { return last_error; }

        void Clear();
    };//class EnvironmentAssetRegistry
}//namespace hgl::graph
