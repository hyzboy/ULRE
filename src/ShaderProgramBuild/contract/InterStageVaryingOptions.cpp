#include <hgl/mtl/InterStageVaryingOptions.h>

#include <cstddef>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr const char *kLocationMacroPrefix = "HGL_LOCATION_";
        constexpr const char *kVaryingMacroPrefix = "HGL_VARYING_";

        /// `HGL_LOCATION_WORLD_POS` → 追加 `#define HGL_VARYING_WORLD_POS 1`。
        ///
        /// 宏名派生而不是查表：`InterStageSemanticRegistry[]` 已经是 location /
        /// 类型 / 插值 / 宏名的单一真源，这里只做一次前缀替换，不复制名单。
        bool AppendVaryingMacro(AnsiString &out, const char *location_macro)
        {
            if (location_macro == nullptr)
                return false;

            const size_t prefix_length = std::strlen(kLocationMacroPrefix);
            if (std::strncmp(location_macro, kLocationMacroPrefix, prefix_length) != 0)
                return false;

            const char *tail = location_macro + prefix_length;
            if (*tail == '\0')
                return false;

            out += "#define ";
            out += kVaryingMacroPrefix;
            out += tail;
            out += " 1\n";
            return true;
        }

        /// `ShaderLibrary/module/stage/interstage_varying.glsl` 已经实现了哪些语义。
        ///
        /// 这不是第二份 location/类型名单（那是注册表），而是「GLSL 侧是否已经
        /// 接上」的接线状态。置位但没接线的语义必须在构建期失败 —— 否则会发出
        /// 一个两端都不认识的开关，退化成本函数出现之前的静默不匹配。
        bool IsWiredInInterStageVaryingGlsl(const InterStageSemantic semantic)
        {
            switch (semantic)
            {
                case InterStageSemantic::DataIndexID:
                case InterStageSemantic::WorldPosition:
                case InterStageSemantic::WorldNormal:
                case InterStageSemantic::UV0:
                case InterStageSemantic::StyleID:
                case InterStageSemantic::Color:
                case InterStageSemantic::FragDirection:
                case InterStageSemantic::Luminance:
                    return true;
                default:
                    // WorldTangent / WorldBinormal 已登记 location 8/9，但既没有
                    // 顶点属性来源（切线由 NTB 模块在**片段侧**从法线+UV 造），
                    // 也没有任何领域契约要求它。接上之前不许置位。
                    return false;
            }
        }
    }//namespace

    bool EmitInterStageVaryingOptions(const InterStageSemanticMask mask,
                                      AnsiString &out_text)
    {
        // 约束 2：顶点色恒发（见头文件）。形状侧不再区分三种顶点色来源，
        // 片段侧除 shadow_caster 外都读它；把它写进 14 条领域契约没有收益。
        const InterStageSemanticMask effective_mask =
            mask | GetInterStageSemanticMask(InterStageSemantic::Color);

        // 约束 1：只发射被置位的语义。按语义枚举升序输出 ⇒ 文本稳定
        //（options 键吃这段文本，顺序漂移会让同一材质分裂出第二个 program）。
        AnsiString text_buffer;
        for (uint32 i = 1; i < 32; ++i)
        {
            if ((effective_mask & (InterStageSemanticMask(1u) << i)) == 0)
                continue;

            const InterStageSemanticInfo *info =
                GetInterStageSemanticInfo(static_cast<InterStageSemantic>(i));
            if (info == nullptr)
            {
                // 契约里出现了注册表没有的语义：宁可在构建期失败，
                // 也不要发出一个两端都无法解释的开关。
                return false;
            }

            if (!IsWiredInInterStageVaryingGlsl(info->semantic))
                return false;

            if (!AppendVaryingMacro(text_buffer, info->location_macro))
                return false;
        }

        if (text_buffer.IsEmpty())
            return false;

        out_text = text_buffer;
        return true;
    }
}// namespace hgl::graph::mtl
