#pragma once

#include<hgl/type/String.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/asset/AssetURI.h>
#include<hgl/asset/AssetSource.h>

namespace hgl
{
    namespace io
    {
        class InputStream;
    }//namespace io

    namespace graph
    {
    /**
     * 资产包裹挂载配置。
     *
     * ## 为什么运行期默认 **不** 回写清单
     *
     * 挂载一个目录包裹时会扫描整棵树并与 `.asset_manifest.toml` 对拍。清单的写入
     * 属于「能够容忍慢」的动作（进版本库前、出包前、CI 闸门），由离线工具
     * `AssetManifestTool` 负责。运行期若也回写，会在启动阶段产生磁盘写、并在
     * 多进程同时启动时互相踩同一个文件 —— 因此默认只读清单。
     *
     * 同理，内容哈希（XXH3-128 / BLAKE3 等）只在离线工具里算：运行期对拍只看
     * `size` / `mtime`，不读文件内容（810MB 的 `res/` 全量哈希不适合放进启动路径）。
     */
    struct AssetSourceMountConfig
    {
        AnsiString  name;                   ///<挂载名（对应 `pack(<name>)`）
        OSString    root;                   ///<目录根（相对当前工作目录或绝对路径）

        bool        build_index     = true; ///<扫描建索引（关掉等于退回「按路径摸文件」）
        bool        load_manifest   = true; ///<挂载前先读 `<root>/.asset_manifest.toml`
        bool        save_manifest   = false;///<对拍不一致时回写清单（运行期默认关）
        bool        only_read       = true; ///<只读标记
    };

    /**
     * 挂载一个「目录包裹」。已存在的同名挂载会被视为成功（幂等）。
     *
     * @param config    挂载配置
     * @param out_error 失败原因（可为 nullptr）
     * @return 挂载成功（含「已挂载」这一情形）返回 true
     */
    bool MountAssetSource(const AssetSourceMountConfig &config,AnsiString *out_error=nullptr);

    /**
     * 挂载引擎默认资产包裹。
     *
     * 当前只有 `res` 一个：把仓库/发行包里的 `res/` 目录登记为
     * `pack(res)`，此后所有资产引用都应写成 `pack(res):<逻辑路径>`
     * （无扩展名）或 `pack(res):id:<32 hex>`。
     *
     * 目录不存在时**不视为错误**：早期示例仍用裸物理路径，缺 `res/` 只是让
     * 包裹寻址不可用，不该阻断启动。
     *
     * @return 至少成功挂载一个包裹返回 true
     */
    bool MountDefaultAssetSources(AnsiString *out_error=nullptr);

    /**
     * @brief 从 TOML 配置文件批量挂载资产包裹（推荐的统一入口）。
     *
     * 挂载是**进程级**的资产寻址能力，与图形设备无关 —— 因此调用方应当是
     * 应用框架（`AppFramework::Init`），而不是 GraphicsContext。配置文件
     * 形如：
     *
     * ```toml
     * [[mount]]
     * name = "res"              # 对应 pack(res)
     * root = "res"              # 相对当前工作目录或绝对路径
     * build_index = true        # 可选，默认 true
     * load_manifest = true      # 可选，默认 true
     * save_manifest = false     # 可选，默认 false
     * ```
     *
     * 逐条走 `MountAssetSource`（同名幂等跳过）；单条失败只告警不中断其余条目，
     * 目录不存在同样不视为致命错误（与 `MountDefaultAssetSources` 的宽容策略一致）。
     *
     * @param path      配置文件路径（OS 路径）
     * @param out_error 打开/解析失败的原因（可为 nullptr）
     * @return 至少成功挂载一个包裹返回 true
     */
    bool MountAssetSourcesFromToml(const OSString &path,AnsiString *out_error=nullptr);

    /// 注销指定包裹（挂载名不存在时静默返回）。
    void UnmountAssetSource(const AnsiString &name);

    /// 注销全部已挂载包裹（框架退出时与 `MountAssetSourcesFromToml` 成对调用）。
    void UnmountAllAssetSources();

    /// 注销全部默认包裹（与 `MountDefaultAssetSources` 成对调用）。
    void UnmountDefaultAssetSources();

    /// 默认包裹的挂载名 / 根目录（供诊断与日志使用）。
    constexpr const char *DEFAULT_ASSET_SOURCE_NAME = "res";
    constexpr const os_char *DEFAULT_ASSET_SOURCE_ROOT = OS_TEXT("res");

    /**
     * 取已挂载的资产来源。
     *
     * 调用方**不要**长期持有返回指针：注销包裹后它就失效了。这里只是为了在同一
     * 帧内做「列举目录」「Stat 元信息」这类 OpenAssetInputStream 覆盖不到的查询。
     *
     * @return 未挂载返回 nullptr
     */
    asset::AssetSource *GetMountedAssetSource(const char *name=DEFAULT_ASSET_SOURCE_NAME);

    /**
     * 列举当前已挂载包裹的挂载名（`pack(<name>)` 的 `<name>` 段）。
     *
     * 存在的理由：`AssetSource` 进程级注册表只支持「按名取源」，而「按短名在
     * 所有包裹的索引里找那一个资产」（材质配方按需加载）需要遍历挂载集合。
     * 名字来自引擎侧挂载记录，与 `GetMountedAssetSource` 严格一致。
     *
     * @param out_names 追加写入，不清空调用方已有内容
     * @return 新增的挂载名数量
     */
    int ListMountedAssetSourceNames(ManagedArray<AnsiString> &out_names);

    /**
     * 列举包裹内某个目录的直接子项。
     *
     * 为什么需要它：材质配方不写在模型包里，场景只给出材质名，配方文件名（
     * `LoadScene.Chess.Chessboard.recipe.toml`）和材质名（`Chessboard`）只有末段
     * 对得上。因此「按材质名找配方」必然要枚举目录 —— 硬编码后缀猜测在换场景时
     * 立刻失效，而且打包后端上根本猜不出内容。
     *
     * @param dir_path 包裹内目录（UTF-8，`/` 分隔，不带挂载名前缀）
     * @param out      追加写入，不清空调用方已有内容
     * @param source_name 挂载名；nullptr 或空串时用默认包裹
     * @return 新增项数；包裹未挂载或后端不支持列举返回 -1
     */
    int ListAssetDirectory(const AnsiString &dir_path,ManagedArray<asset::SourceEntry> &out,
                           const char *source_name=nullptr);

    /**
     * 统一资产取流入口（包裹 / 磁盘 / 历史裸路径三者的唯一分派点）。
     *
     * ── 为什么 loader 不能自己 new FileInputStream ─────────────────────────────
     * 挂载了 `pack(res)` 之后，「一个资产字节从哪里读」不再只有一个答案：可能是
     * 挂载目录，也可能是将来的打包文件。任何绕过本函数的读盘路径在切到打包文件
     * 后端时都会静默失效 —— 因此**所有需要读取资产字节的代码都必须走这里**。
     *
     * ── 分派 ──────────────────────────────────────────────────────────────────
     *   · `pack(<name>):<target>` → 查挂载表；
     *       - `<target>` 是 `id:<32 hex>` ⇒ 按身份寻址（不回落）
     *       - 否则归一化成包裹内路径，推导 (类型, 逻辑路径) 走索引；有索引却查不到
     *         ⇒ 当场失败（不退回按路径摸文件，避免开发期/发行期行为分叉）
     *       - 推导不出类型（原始素材）⇒ 按调用方给的那一个路径直通
     *   · `file:<path>` / 裸相对路径 → 本地文件
     *   · `builtin` / `rt`          → 不是字节流来源，返回 nullptr
     *
     * 带盘符的裸路径（`C:\a\b.scene`）会被 URI 解析器拆出错误的定位段，本函数对
     * 这种「来源仍是 Unknown 且定位段已不等于原文」的情形退回按原串开本地文件，
     * 因此旧代码把任意物理路径塞进来都不会退化。
     *
     * @param uri_text     资产 URI 或裸物理路径
     * @param default_type 文本里省略类型段时补上的类型（只影响诊断，不影响取流）
     * @param out_error    失败原因（可为 nullptr）
     * @return 调用方负责 delete；失败返回 nullptr
     */
    io::InputStream *OpenAssetInputStream(const OSString &uri_text,
                                          asset::AssetType default_type=asset::AssetType::Unknown,
                                          AnsiString *out_error=nullptr);

    /// 已解析 URI 的重载（调用方已经拆好段时用）。
    io::InputStream *OpenAssetInputStream(const asset::AssetURI &uri,
                                          AnsiString *out_error=nullptr);

    /**
     * @brief 把资产 URI 解析成**物理磁盘路径**（不解引用、不读字节）。
     *
     * 存在的唯一理由：异步读盘（`LoadedFileManager::Submit`）只接受物理路径，
     * 而资产可能位于 `pack(<name>)` 挂载里。有了本函数，包裹资产也能走异步 IO，
     * 不必为了拿到一个路径把整包同步读进内存。
     *
     * ── 解析次序 ──────────────────────────────────────────────────────────────
     *   · `pack(<name>)` + 索引存在时，先按索引反查：
     *     `id:<32 hex>` → 由 `AssetSource::ResolvePath(AssetID)` 取包内相对路径；
     *     带类型段 → 由 `AssetSource::ResolvePath(type,logical)` 取包内相对路径。
     *     索引已经裁决过 `.toml`(源) / `.fb`(烘焙) 同名撞车，因此这条通道拿到的是
     *     **当前该用的那个文件**，而不是「拼出来的名字恰好存在」的那个；
     *   · 索引无该条目（原始素材，类型段就是物理后缀）才退回直通拼接；
     *   · 裸物理路径原样校验存在性。
     *
     * ── 局限 ──────────────────────────────────────────────────────────────────
     *   · `builtin` / `rt` / 匿名 `pack` 没有磁盘对应物 ⇒ 返回 false；
     *   · `id:<32 hex>` 在索引缺失或索引里没有该身份时 ⇒ 返回 false
     *     （此时请改用 `OpenAssetInputStream` 取流）；
     *   · 只适用于「包裹根目录直接就是文件树」的目录型包裹。真正的归档型包裹
     *     （单文件 `.pack`）没有可寻址的物理路径，必须走
     *     `LoadedFileManager::Submit` + `asset::AssetURIByteSource` 的取流通道
     *     （路由路径可用下面的 `GetAssetMountRoot`），不能靠本函数。
     *
     * @param uri_text     资产 URI 或裸物理路径
     * @param default_type 文本里省略类型段时补上的类型
     * @param out_path     解析结果（失败时被清空）
     * @return 路径存在且是文件返回 true
     */
    bool ResolveAssetPhysicalPath(const OSString &uri_text,
                                  asset::AssetType default_type,
                                  OSString &out_path);

    /// 同上，默认类型为 `Unknown`。
    inline bool ResolveAssetPhysicalPath(const OSString &uri_text,OSString &out_path)
    {
        return ResolveAssetPhysicalPath(uri_text,asset::AssetType::Unknown,out_path);
    }

    /**
     * 取已挂载包裹的物理根目录。
     *
     * 归档型包裹里的资产没有可直接寻址的路径，但读盘线程仍需要知道「这份字节
     * 落在哪块物理盘上」——挂载根本身就在正确的盘上，用它给
     * `asset::AssetURIByteSource` 提供路由路径即可。未挂载返回 false。
     */
    bool GetAssetMountRoot(const AnsiString &name,OSString &out_root);

    }//namespace graph
}//namespace hgl
