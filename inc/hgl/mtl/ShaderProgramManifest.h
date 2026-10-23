/**
 * ShaderProgramManifest.h —— 发行形态的「语义键 → program link」查表清单。
 *
 * 为什么需要它：
 *   运行时要拿到一个 `ShaderProgram`，必须先有 `ShaderProgramKey`。开发形态靠
 *   生成器算键——键的输入包含 GLSL 声明闭包（读 ShaderLibrary/*.glsl 的模块
 *   源 + 接口契约），发行形态没有生成器也没有 GLSL 源码。
 *
 *   所以烘焙期把「运行时能自己算出来的全部语义维度」折成一个轻量键，记下它
 *   对应的 program link；发行期用同一个键查表，直接得到键与各 stage SPV。
 *
 * 键必须**完整决定** program key（见 `ComputeMaterialProgramManifestKey`），
 * 否则会静默取到不匹配的 SPV。查不到一律硬失败，绝不猜。
 *
 * 本头只做数据容器与磁盘格式，不含 GLSL / 编译器插件。文件 IO 用 HGL。
 */
#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/ShaderArtifactContract.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    struct ShaderProgramManifestEntry
    {
        /// 轻量语义键（`ComputeMaterialProgramManifestKey` 产出）。
        uint64 lookup_key = 0;
        ShaderLinkSpec program_link;
        /// 材质是否需要 per-instance 运行时数据行；消费者是渲染准备阶段。
        bool requires_recipe_runtime_rows = false;
        /// 程序是否读 payload 行（`MTL_ROW`）——即 recipe 是否必须给出
        /// `material_ssbo_binding`。纯纹理材质为 false。消费者 = 渲染准备阶段的
        /// 材质私有数据 SSBO 绑定校验。
        bool requires_recipe_payload_row = false;

        bool operator==(const ShaderProgramManifestEntry &rhs) const noexcept
        {
            return lookup_key == rhs.lookup_key
                && requires_recipe_runtime_rows == rhs.requires_recipe_runtime_rows
                && requires_recipe_payload_row == rhs.requires_recipe_payload_row
                && program_link.mesh_stage == rhs.program_link.mesh_stage
                && program_link.fragment_stage == rhs.program_link.fragment_stage
                && program_link.resource_layout_hash == rhs.program_link.resource_layout_hash
                && program_link.vertex_input_hash == rhs.program_link.vertex_input_hash
                && program_link.render_target_hash == rhs.program_link.render_target_hash
                && program_link.compiler_hash == rhs.program_link.compiler_hash;
        }

        bool operator!=(const ShaderProgramManifestEntry &rhs) const noexcept
        {
            return !(*this == rhs);
        }
    };

    class ShaderProgramManifest
    {
        uint64 compiler_profile_hash = 0;
        uint32 codegen_generation = 0;
        ValueArray<ShaderProgramManifestEntry> entries;

    public:
        ShaderProgramManifest() = default;

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
        const ShaderProgramManifestEntry *Find(const uint64 lookup_key) const;

        /// 插入或覆盖同一键的条目。返回值 = 内容是否真的变了（false = 原样命中，
        /// 调用方据此决定要不要重写文件）。
        bool Upsert(const ShaderProgramManifestEntry &entry);

        /// 读盘。格式/版本/校验和/代码生成代数任一不符即为失败（并按空清单处理）。
        bool Load(const OSString &path);

        /// 落盘。清单很小（每种材质变体一行），整份重写。
        bool Save(const OSString &path) const;
    };

    /// 清单默认路径：`<cache_root>/program_manifest.pmf`（与 stage/ program/ 平级）。
    OSString MakeShaderProgramManifestPath(const OSString &cache_root);
}//namespace hgl::graph::mtl
