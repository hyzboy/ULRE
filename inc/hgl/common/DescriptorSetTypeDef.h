#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/StrChar.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    /// 注：Set 0（Scene 集）与其绑定枚举 `SceneBinding` 已**整体退场**——相机 / 调色板 /
    /// 全局地址表 / sky / viewport / shadow 全部改走 BDA（地址进 global_addresses 表，
    /// 表基址经 pc_root.addr_global_addresses 下发）⇒ 本引擎不再有任何 Scene 集绑定，
    /// 描述符集只剩 Bindless 一个（集号随之收敛为 0）。

    enum class DescriptorSetType:int
    {
        Unknown=-1,        ///<Phase 7 拼写修正：Unknown（枚举值不变，序列化契约不受影响）

        Bindless=0,     ///< 全局 Bindless 纹理数组集合（Set 0），一帧绑一次
                        ///< （Scene/PerObject/Vertex/Material 集已随 BDA 化全部退场——行表/顶点流/
                        ///<  材质行经 pc_root 地址 + buffer_reference 寻址，viewport / sky / shadow
                        ///<  亦走 global_addresses ⇒ 唯一集合就是本集）

        ENUM_CLASS_RANGE(Bindless,Bindless)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    /// 按索引调试名（Scene 集退场后唯一集合：Bindless=0）
    constexpr const char *DescriptSetTypeName[]=
    {
        "Bindless"
    };

    inline const char *GetDescriptorSetTypeName(const enum class DescriptorSetType &type)
    {
        if(type==DescriptorSetType::Unknown)return "Unknown";

        RANGE_CHECK_RETURN_NULLPTR(type);

        return DescriptSetTypeName[(size_t)type];
    }

    /// 宏类别：DescriptorMacroGen 生成器按此决定 #define 的输出形态
    enum class DescriptorMacroKind
    {
        SetIndex,   ///< #define <name> <集合序号>        如 BINDLESS_SET 0
        SetAlias,   ///< #define <name> <alias_target>    （PerObject 别名宏已随集退场，暂无可选项）
        Binding     ///< #define <name> <绑定号>          如 VERTEX_POSITION_BINDING 4
    };

    /// 描述符宏规范——ShaderLibrary/common/descriptor_macros.glsl 生成器的唯一输入。
    /// 每行对应生成文件中的一个 #define（含其上的注释）；数组行序即输出行序。
    ///
    /// 宏名与 Binding 枚举名不做机械推导（历史拼写不规则：TRANSFORMID/CHARINFO），
    /// 一律在此表显式给出；修改绑定号只改枚举，宏文本只改本表，再重新生成 .glsl。
    ///
    /// blank_before/blank_after_comment 用于逐字节复刻现行手写文件的空行布局
    ///（该文件文本经模块系统拼入 FinalGLSL 参与内容哈希，格式必须稳定）。
    struct DescriptorBindingMacroSpec
    {
        DescriptorMacroKind kind;
        DescriptorSetType set_type;    ///< 宏归属集合（SetIndex 行其值即宏值；Binding 行为绑定所在集合）
        const char *name;              ///< SetIndex/SetAlias：集合宏名；Binding：绑定宏名
        const char *alias_target;      ///< 仅 SetAlias：目标集合宏名；其余 nullptr
        int binding;                   ///< 仅 Binding：绑定号（取自 Binding 枚举）；其余 -1
        const char *comment;           ///< 输出在该宏之前的注释（可含 '\n' 表多行，行内自带 "//"；nullptr 表示无）
        bool blank_before        = true;  ///< 本条目之前输出一个空行（连续绑定宏块为 false）
        bool blank_after_comment = false; ///< 注释之后再输出一个空行（区块标题历史格式，仅 3 处）
    };

    /// 与 descriptor_macros.glsl 一一对应（该 .glsl 为 DescriptorMacroGen 生成物）。
    constexpr const DescriptorBindingMacroSpec kDescriptorBindingMacros[]=
    {
        {DescriptorMacroKind::SetIndex,DescriptorSetType::Bindless, "BINDLESS_SET",              nullptr,                                   -1,
            "// ── Descriptor Set 索引 ──",                            true, true},
    };

    /// 统一发射所有描述符集/绑定宏（由 C++ 单源表 kDescriptorBindingMacros 驱动）
    template <typename StringType>
    inline void EmitDescriptorBindingDefines(StringType &out)
    {
        for (const auto &spec : kDescriptorBindingMacros)
        {
            out += "#ifndef ";
            out += spec.name;
            out += "\n#define ";
            out += spec.name;
            out += " ";

            char num_buf[32];
            switch (spec.kind)
            {
            case DescriptorMacroKind::SetIndex:
                snprintf(num_buf, sizeof(num_buf), "%d", int(spec.set_type));
                out += num_buf;
                break;

            case DescriptorMacroKind::SetAlias:
                if (spec.alias_target)
                    out += spec.alias_target;
                break;

            case DescriptorMacroKind::Binding:
                snprintf(num_buf, sizeof(num_buf), "%d", spec.binding);
                out += num_buf;
                break;
            }

            out += "\n#endif\n";
        }
    }
}//namespace hgl::graph
