#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/StrChar.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    /// Scene 集（Set 0）UBO 绑定号。
    /// 绑定号即 ABI：数值必须显式写死，禁止省略 "=值" 依赖编译器自动续号
    ///（中间插入新条目会导致后续全部静默重编号，破坏已编译着色器缓存）。
    enum class SceneBinding : int
    {
        Camera=0,        ///< 相机 UBO
        Sky=1,           ///< 天空/太阳光 UBO
        Viewport=2,      ///< 视口 UBO
        ColorPalette=3,  ///< 顶点调色板 UBO

        ENUM_CLASS_RANGE(Camera,ColorPalette)  ///< RANGE_SIZE 供资源目录覆盖性断言（漏登记即编译失败）
    };

    /// ABI 锚点：以下数值被 ShaderLibrary/common/descriptor_macros.glsl 与运行时绑定表依赖，
    /// 变更即破坏全部已编译着色器；static_assert 保证插入新条目引发的静默重编号在编译期暴露。
    static_assert(int(SceneBinding::Camera)==0
               && int(SceneBinding::Viewport)==2,
                  "Scene UBO binding ABI changed");

    /// ── 兼容别名：既有调用点继续使用 kXxx 常量名，数值真源已上收至上述枚举 ──
    constexpr const int kSceneBindingCamera       = int(SceneBinding::Camera);        ///< 相机 UBO
    constexpr const int kSceneBindingSky          = int(SceneBinding::Sky);           ///< 天空/太阳光 UBO
    constexpr const int kSceneBindingViewport     = int(SceneBinding::Viewport);      ///< 视口 UBO
    constexpr const int kSceneBindingColorPalette = int(SceneBinding::ColorPalette);  ///< 顶点调色板 UBO

    enum class DescriptorSetType:int
    {
        Unknown=-1,        ///<Phase 7 拼写修正：Unknown（枚举值不变，序列化契约不受影响）

        Scene=0,        ///< 全局 UBO 集（camera/sky/viewport/color_palette），所有材质共用，一帧写/绑一次
        Bindless=1,     ///< 全局 Bindless 纹理数组集合（Set 1），一帧绑一次
                        ///< （PerObject/Vertex/Material 集已随 BDA 化退场——行表/顶点流/材质行
                        ///<  均经 pc_root 地址 + buffer_reference 寻址，无 per-material 描述符；
                        ///<  b3 集号收敛：Bindless 2→1，终态两集 Scene(0)/Bindless(1)）

        ENUM_CLASS_RANGE(Scene,Bindless)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    /// 按索引调试名（b3 集号收敛后两集：Scene=0/Bindless=1）
    constexpr const char *DescriptSetTypeName[]=
    {
        "Scene",
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
        SetIndex,   ///< #define <name> <集合序号>        如 SCENE_SET 0
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
        {DescriptorMacroKind::SetIndex,DescriptorSetType::Scene,    "SCENE_SET",                 nullptr,                                   -1,
            "// ── Descriptor Set 索引 ──",                            true, true},

        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "CAMERA_BINDING",            nullptr,   int(SceneBinding::Camera),
            "// ── Scene set ──",                                      true, true},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "SKY_BINDING",               nullptr,   int(SceneBinding::Sky),                    nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "VIEWPORT_BINDING",          nullptr,   int(SceneBinding::Viewport),               nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "COLOR_PALETTE_BINDING",     nullptr,   int(SceneBinding::ColorPalette),           nullptr},

        {DescriptorMacroKind::SetIndex,DescriptorSetType::Bindless, "BINDLESS_SET",              nullptr,                                   -1, nullptr},
    };
}//namespace hgl::graph
