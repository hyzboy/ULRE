#pragma once

#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/new/MaterialPresetDef.h>
#include <vector>
#include <map>

namespace hgl::graph
{
    struct SPVData;   // forward decl from GLSLCompiler.h

    /**
     * SPV 编译输出
     */
    struct CompiledSPV
    {
        std::vector<uint32_t>   vertex_spv;
        std::vector<uint32_t>   fragment_spv;
        std::string             error_message;
        bool                    success = false;
    };

    /**
     * SPV 缓存键 — 唯一标识一个编译后的 shader 变体
     */
    struct SPVCacheKey
    {
        uint16_t    preset_id;
        uint16_t    packed_key;     // NewShaderPermutationKey::packed
        PassType    pass_type;

        bool operator<(const SPVCacheKey &o) const
        {
            if (preset_id != o.preset_id) return preset_id < o.preset_id;
            if (packed_key != o.packed_key) return packed_key < o.packed_key;
            return static_cast<uint8_t>(pass_type) < static_cast<uint8_t>(o.pass_type);
        }

        bool operator==(const SPVCacheKey &o) const
        {
            return preset_id == o.preset_id
                && packed_key == o.packed_key
                && pass_type == o.pass_type;
        }
    };

    /**
     * PresetShaderCompiler — 遍历 MaterialPresetDef 列表，编译所有排列组合为 SPV
     *
     * 第一版流程：
     *   1. 对每个 Preset × 有效的 NewShaderPermutationKey 组合
     *   2. 调用 CompositorAssembler::Assemble() 生成 GLSL
     *   3. 调用现有的 GLSLCompiler 编译为 SPV
     *   4. 输出 {preset_id, key} → SPV binary 映射
     */
    class PresetShaderCompiler
    {
    public:

        explicit PresetShaderCompiler(const CompositorAssembler &assembler);

        /// 编译单个 preset + key + pass 组合
        CompiledSPV CompileOne(
            const MaterialPresetDef &preset,
            const NewShaderPermutationKey &key,
            PassType pass
        ) const;

        /// 编译所有 preset × 所有有效 key 组合，结果存入 out_map
        bool CompileAll(
            const MaterialPresetDef *presets,
            size_t preset_count,
            std::map<SPVCacheKey, CompiledSPV> &out_map,
            std::string &out_error
        ) const;

    private:

        const CompositorAssembler &assembler_;
    };
}
