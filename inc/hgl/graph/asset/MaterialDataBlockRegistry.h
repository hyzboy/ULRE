#pragma once

/**
 * 材质数据块注册表 —— 「`mtldata:` 的 URI 进，句柄出」的门面。
 *
 * 与 `MaterialRecipeRegistry` 同构：表拥有句柄（`ManagedArray`，对外一律裸观察
 * 指针），加载入口按扩展名分派，查询覆盖 名字 / 路径 / URI 三种定位方式。
 *
 * ── 三种文件形态都是一个东西 ────────────────────────────────────────────────
 *
 * | 扩展名 | 读法 | 是否带授权形态 | 典型产出者 |
 * |--------|------|----------------|------------|
 * | `.material_data.toml` | `ParseMaterialDataBlockFile` | 有 | 手写 / 示例数据 |
 * | `.material_data.mdb`  | `ReadMaterialDataBlockBinary`（`MDB1`） | 无 | 烘焙器、离线工具 |
 * | `.material_data.fb`   | `DecodeMaterialDataBlockFlat` | 有 | 程序生成（可回读改行） |
 *
 * 三者读完都变成同一份句柄 + 同一份物理行，**上传路径完全共用**；差别只在
 * 「检查器能不能看到字段语义」。`MDB1` 之所以存在，是因为它比 FlatBuffers 更贴近
 * 显存布局 —— 16 字节定长头之后就是纯行区，理论上可以 memcpy 直通。
 *
 * ── 没有图形设备也能用 ──────────────────────────────────────────────────────
 *
 * `LoadFile` / `LoadDirectory` 只做解析与建句柄，不碰 GPU。`UploadAll` 才需要
 * `GlobalSSBOBufferRegistry`，而它传 nullptr 时是**软失败**（句柄停在 `Parsed`）。
 * 于是同一套注册表既能给离线工具做数据校验，也能在设备起来后一键上船。
 *
 * 线程约定：主线程；与所有注册表一致，不做并发保护。
 */

#include <hgl/asset/AssetURI.h>
#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/type/ManagedArray.h>

namespace hgl::graph::mtl
{
    class MaterialDataBlockRegistry
    {
        ManagedArray<MaterialDataBlockAsset> blocks;

        /// 把刚解析好的数据挂进表（唯一建句柄的地方，避免 `new` 散落各处）
        MaterialDataBlockAsset *Attach(MaterialDataBlockFileData &&data,
                                       MaterialDataBlockSpec      &&spec,
                                       bool                         has_spec,
                                       const os_char               *label);

    public:

        ~MaterialDataBlockRegistry() = default;

        /**
         * TOML 文本 → 句柄。
         *
         * 解析失败也**会**建出句柄并留在表里（`Failed` 态 + `GetParseError()`），
         * 这与「坏配方从表里摘掉」不同：数据块的诊断信息（行号 / 键 / 值）本身
         * 就是工具要显示的内容，摘掉就没东西可显示了。
         */
        MaterialDataBlockAsset *RegisterFromToml(const os_char *label,
                                                 const char *content,
                                                 int content_size);

        /// `MDB1` 字节 → 句柄（坏 magic / 版本 / 行宽 ⇒ `Failed`）
        MaterialDataBlockAsset *RegisterFromBinary(const os_char *label,
                                                   const uint8 *data,
                                                   uint64 size);

        /// FlatBuffers 缓冲 → 句柄（先校验后解码；`Failed` 时 `GetFailureReason()` 有原因）
        MaterialDataBlockAsset *RegisterFromFlat(const os_char *label,
                                                 const uint8 *data,
                                                 uint64 size);

        /// 按扩展名分派读一个文件
        bool LoadFile(const OSString &path);

        /// 扫目录（递归），只收三种合法扩展名
        bool LoadDirectory(const OSString &directory,
                           int *out_file_count = nullptr,
                           int *out_error_count = nullptr);

        /**
         * 把所有尚未上船的句柄整块上传。
         *
         * @param registry 全局池；`nullptr` ⇒ 全部跳过并返回 0（不标 `Failed`）
         * @return 本次新上传成功的块数
         */
        int UploadAll(GlobalSSBOBufferRegistry *registry);

        /**
         * URI 门面：`mtldata:@<name>` 查表，`mtldata:file:<path>` / 裸路径按需加载。
         *
         * 类型段写了别的类型（`geo:...`）⇒ 明确报错返回 nullptr，不当路径去读盘。
         */
        MaterialDataBlockAsset *GetOrLoadURI(const OSString &uri);

        /// 按块名查（`spec.name`；无名块查不到）
        MaterialDataBlockAsset *FindByName(const char *name);

        /// 按来源路径查（内存注册的块没有路径，恒不命中）
        MaterialDataBlockAsset *FindAssetByPath(const OSString &path);

        /// 按下标取（越界返回 nullptr）
        MaterialDataBlockAsset *GetAssetAt(int index) { return blocks.At(index); }

        int GetCount() const { return blocks.GetCount(); }

        /// 清空表（句柄析构会把各自的区间租约归还，见 `GlobalSSBORowRangeLease`）
        void Clear() { blocks.Clear(); }

    private:

        /// `@name` 形式的名字查询：`OSString` 与句柄里的 `AnsiString` 用宽窄转换比较，
        /// 不假设两边编码相同。
        MaterialDataBlockAsset *FindByNameOS(const OSString &name);
    };//class MaterialDataBlockRegistry
}//namespace hgl::graph::mtl
