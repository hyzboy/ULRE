/**
 * ShaderComputeProgramManifest.h —— compute 程序的「名字 → SPV」发行形态查表。
 *
 * 为什么需要它：
 *   材质程序走 program_manifest.pmf，键是材质的语义维度。compute 程序（GPU cull
 *   之类）没有材质定义，也没有 mesh/fragment 两段 link，键也不是材质语义 ——
 *   它只需要回答「叫这个名字的程序，烘焙期到底编出了哪段 SPV」。
 *
 *   把这件事硬塞进材质清单会让定长记录长出两种形态、让 `ShaderLinkSpec::IsValid`
 *   多一条 compute 分支，还会让「一个 compute 程序损坏」牵连整份材质清单作废。
 *   所以单独一份 `compute_manifest.pmf`，header / stamp / 校验和机制与材质清单
 *   完全同构（`ShaderArtifactContract.h` 里同一套常量和结构体）。
 *
 * 内容寻址：entry 里的 compute stage key 不是「声明哈希」——发行形态没有声明源，
 * 算不出来。key 由**烘焙期**按程序名与 SPV 字节内容填出（见
 * `MakeComputeStageKey`），于是 SPV 一变文件名就变，旧文件自然退场，
 * 不需要额外的失效逻辑。
 *
 * 本头只做数据容器与磁盘格式 + 两个产物读写函数。零 GLSL、零 glslang，
 * 发行形态照常链接。
 */
#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/mtl/ShaderArtifactContract.h>
#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    class ShaderArtifactStore;

    struct ShaderComputeProgramManifestEntry
    {
        /// 程序名 digest（`ComputeShaderComputeProgramManifestKey`）。运行时可算。
        uint64 program_key = 0;
        /// compute stage 的 SPV 定位键。字段来源见 `MakeComputeStageKey`。
        ShaderStageKey compute_stage;
        /// 烘焙期创建管线时用的 push constant 字节数；0 = 该程序不用 push constant。
        /// 运行时候必须与之一致 —— 不一致意味着 layout 与 SPV 的偏移约定脱节。
        uint32 push_constant_size = 0;

        bool operator==(const ShaderComputeProgramManifestEntry &rhs) const noexcept
        {
            return program_key == rhs.program_key
                && compute_stage == rhs.compute_stage
                && push_constant_size == rhs.push_constant_size;
        }

        bool operator!=(const ShaderComputeProgramManifestEntry &rhs) const noexcept
        {
            return !(*this == rhs);
        }

        bool IsValid() const noexcept
        {
            return program_key != 0
                && compute_stage.stage == ShaderStage::Compute
                && push_constant_size <= 128;   // Vulkan spec 保证的 maxPushConstantsSize 下限
        }
    };

    class ShaderComputeProgramManifest
    {
        uint64 compiler_profile_hash = 0;
        uint32 codegen_generation = 0;
        ValueArray<ShaderComputeProgramManifestEntry> entries;

    public:
        ShaderComputeProgramManifest() = default;

        void Clear();

        bool IsEmpty() const { return entries.IsEmpty(); }
        uint32 GetEntryCount() const { return static_cast<uint32>(entries.GetCount()); }

        uint64 GetCompilerProfileHash() const noexcept { return compiler_profile_hash; }
        uint32 GetCodegenGeneration() const noexcept { return codegen_generation; }

        void SetStamps(const uint64 profile_hash, const uint32 generation) noexcept
        {
            compiler_profile_hash = profile_hash;
            codegen_generation = generation;
        }

        /// 找不到返回 nullptr。
        const ShaderComputeProgramManifestEntry *Find(const uint64 program_key) const;

        /// 插入或覆盖同一键的条目。返回值 = 内容是否真的变了（false = 原样命中）。
        bool Upsert(const ShaderComputeProgramManifestEntry &entry);

        /// 读盘。格式/版本/校验和任一不符即失败（并按空清单处理）。
        bool Load(const OSString &path);

        /// 落盘。条目以程序个数计，整份重写。
        bool Save(const OSString &path) const;
    };

    /// 程序名 → 查表键。烘焙期与运行期必须同一式，故只有这一份实现。
    uint64 ComputeShaderComputeProgramManifestKey(const char *program_name);

    /// compute stage key：把「程序名 + SPV 字节 + 编译器身份」编成 SPV 文件名键。
    /// 发行形态不需要重算它 —— 键直接来自清单，此处供烘焙期构造。
    ShaderStageKey MakeComputeStageKey(
        const char *program_name,
        const void *spv_data,
        const uint64 spv_size,
        const uint64 compiler_hash);

    /// compute manifest 默认路径：`<cache_root>/shader-cache/compute_manifest.pmf`。
    OSString MakeShaderComputeProgramManifestPath(const OSString &cache_root);

    /**
     * 烘焙：把一段 compute SPV 写进 store 的 stage 目录，并在 compute manifest 里
     * 登记「程序名 → 该 stage」。只读 store 直接返回 false（绝不回写）。
     *
     * compiler_hash 省略时按当前编译目标常量算（`contract::GetShaderCompilerProfileHash`）；
     * 显式传入只为让回归门能把「编译器身份不同则内容应不同」做成可观察事实。
     */
    bool RecordShaderComputeProgramArtifact(
        ShaderArtifactStore &store,
        const char *program_name,
        const void *spv_data,
        const uint64 spv_size,
        const uint32 push_constant_size,
        const uint64 compiler_hash = 0);

    /**
     * 发行：按程序名只读查表取 compute SPV。
     *
     * 未命中一律返回 false（不猜、不回落）。清单的 codegen 代数与本进程不符时
     * 同样视为未命中 —— 旧代数的 SPV 与当前 layout 约定可能不兼容。
     * out_push_constant_size 可为 nullptr；非空时回填清单记录值，由调用方比对。
     */
    bool LoadShaderComputeProgramArtifact(
        ShaderArtifactStore &store,
        const char *program_name,
        ValueArray<uint8> &out_spv,
        uint32 *out_push_constant_size = nullptr);
}//namespace hgl::graph::mtl
