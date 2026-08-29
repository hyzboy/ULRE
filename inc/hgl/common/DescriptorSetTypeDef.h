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
    };

    /// PerObject 集（Set 1）编译期固定 SSBO 绑定号。
    /// 成员为引擎内部 per-object 结构，非 TOML 动态列表；历史跳号（2、7）保持不变。
    enum class PerObjectBinding : int
    {
        L2W              = 0,   ///< per-draw 变换数据
        L2WIndex         = 1,   ///< 实例 → l2w 行索引表
        PrivateDataIndex = 3,   ///< 实例 → 材质私有数据行索引表（MaterialPrivateDataIndex）

        // ── 过渡期临时条目：顶点数据 SSBO 现居 PerObject 集；
        //    Phase 5 迁至 Vertex 集后删除本组（见 ShaderGen_Descriptor_ABI_Unification_Plan.md）──
        VertexPosition   = 4,   ///< 顶点位置 SSBO
        VertexUV         = 5,   ///< 顶点 UV SSBO
        VertexNTB        = 6,   ///< 顶点 NTB SSBO
        VertexIndex      = 8,   ///< 顶点索引 SSBO
        VertexColor      = 9,   ///< 顶点颜色 SSBO
        VertexLuminance  = 10,  ///< 顶点亮度 SSBO
        VertexTransformID= 11,  ///< 顶点 TransformID SSBO（调色板变换索引）
        VertexSize       = 12,  ///< 顶点 Size/宽度 SSBO（Line width）

        MeshDrawParams   = 13,  ///< mesh per-draw 参数表 SSBO（indirect 合批查表）

        // CharQuad 文本字符 SSBO（TextCharQuad mesh shader 模式）
        TextCharInfo     = 14,  ///< 字符信息 SSBO
        TextCharStyle    = 15,  ///< 字符样式 SSBO
        TextCharInstance = 16,  ///< 字符实例 SSBO
    };

    /// Vertex 集（Set 4）绑定号——Phase 5 引入 Vertex 集时在此定义：
    /// Position=0, UV=1, NTB=2, Index=3, Color=4, Luminance=5, TransformID=6, Size=7
    enum class VertexBinding : int;

    /// ABI 锚点：以下数值被 ShaderLibrary/common/descriptor_macros.glsl 与运行时绑定表依赖，
    /// 变更即破坏全部已编译着色器；static_assert 保证插入新条目引发的静默重编号在编译期暴露。
    static_assert(int(SceneBinding::Camera)==0
               && int(SceneBinding::Viewport)==2,
                  "Scene UBO binding ABI changed");

    static_assert(int(PerObjectBinding::L2W)==0
               && int(PerObjectBinding::PrivateDataIndex)==3
               && int(PerObjectBinding::MeshDrawParams)==13
               && int(PerObjectBinding::TextCharInstance)==16,
                  "PerObject binding ABI changed");

    /// ── 兼容别名：既有调用点继续使用 kXxx 常量名，数值真源已上收至上述枚举 ──
    constexpr const int kSceneBindingCamera       = int(SceneBinding::Camera);        ///< 相机 UBO
    constexpr const int kSceneBindingSky          = int(SceneBinding::Sky);           ///< 天空/太阳光 UBO
    constexpr const int kSceneBindingViewport     = int(SceneBinding::Viewport);      ///< 视口 UBO
    constexpr const int kSceneBindingColorPalette = int(SceneBinding::ColorPalette);  ///< 顶点调色板 UBO

    constexpr const int kPerObjectBindingL2W               = int(PerObjectBinding::L2W);              ///< per-draw 变换数据
    constexpr const int kPerObjectBindingL2WIndex          = int(PerObjectBinding::L2WIndex);         ///< 实例 → l2w 行索引表
    constexpr const int kPerObjectBindingPrivateDataIndex  = int(PerObjectBinding::PrivateDataIndex); ///< 实例 → 材质私有数据行索引表
    constexpr const int kPerObjectBindingVertexPosition    = int(PerObjectBinding::VertexPosition);   ///< 顶点位置 SSBO
    constexpr const int kPerObjectBindingVertexUV          = int(PerObjectBinding::VertexUV);         ///< 顶点 UV SSBO
    constexpr const int kPerObjectBindingVertexNTB         = int(PerObjectBinding::VertexNTB);        ///< 顶点 NTB SSBO
    constexpr const int kPerObjectBindingVertexIndex       = int(PerObjectBinding::VertexIndex);      ///< 顶点索引 SSBO
    constexpr const int kPerObjectBindingVertexColor       = int(PerObjectBinding::VertexColor);      ///< 顶点颜色 SSBO
    constexpr const int kPerObjectBindingVertexLuminance   = int(PerObjectBinding::VertexLuminance);  ///< 顶点亮度 SSBO
    constexpr const int kPerObjectBindingVertexTransformID = int(PerObjectBinding::VertexTransformID);///< 顶点 TransformID SSBO
    constexpr const int kPerObjectBindingVertexSize        = int(PerObjectBinding::VertexSize);       ///< 顶点 Size/宽度 SSBO
    constexpr const int kPerObjectBindingMeshDrawParams    = int(PerObjectBinding::MeshDrawParams);   ///< mesh per-draw 参数表 SSBO
    constexpr const int kPerObjectBindingTextCharInfo      = int(PerObjectBinding::TextCharInfo);     ///< 字符信息 SSBO
    constexpr const int kPerObjectBindingTextCharStyle     = int(PerObjectBinding::TextCharStyle);    ///< 字符样式 SSBO
    constexpr const int kPerObjectBindingTextCharInstance  = int(PerObjectBinding::TextCharInstance); ///< 字符实例 SSBO

    enum class DescriptorSetType:int
    {
        Unknow=-1,

        Scene=0,        ///< 全局 UBO 集（camera/sky/viewport/color_palette），所有材质共用，一帧写/绑一次
        PerObject,      ///< per-object/per-draw SSBO 集（l2w/l2w_index_rows/joint/material_private_data_index_rows）
        Material,       ///< per-material 描述符集（mtl 数据槽/索引表）
        Bindless,       ///< 全局 Bindless 纹理数组集合（Set 3），一帧绑一次

        ENUM_CLASS_RANGE(Scene,Bindless)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    constexpr const char *DescriptSetTypeName[]=
    {
        "Scene",
        "PerObject",
        "Material",
        "Bindless"
    };

    inline const char *GetDescriptorSetTypeName(const enum class DescriptorSetType &type)
    {
        if(type==DescriptorSetType::Unknow)return "Unknow";

        RANGE_CHECK_RETURN_NULLPTR(type);

        return DescriptSetTypeName[(size_t)type];
    }

    /// 宏类别：DescriptorMacroGen 生成器按此决定 #define 的输出形态
    enum class DescriptorMacroKind
    {
        SetIndex,   ///< #define <name> <集合序号>        如 SCENE_SET 0
        SetAlias,   ///< #define <name> <alias_target>    如 L2W_SET PER_OBJECT_SET
        Binding     ///< #define <name> <绑定号>          如 VERTEX_POSITION_BINDING 4
    };

    /// 描述符宏规范——ShaderLibrary/common/descriptor_macros.glsl 生成器的唯一输入。
    /// 每行对应生成文件中的一个 #define（含其上的注释）；数组行序即输出行序。
    ///
    /// 宏名与 Binding 枚举名不做机械推导（历史拼写不规则：TRANSFORMID/CHARINFO），
    /// 一律在此表显式给出；修改绑定号只改枚举，宏文本只改本表，再重新生成 .glsl。
    struct DescriptorBindingMacroSpec
    {
        DescriptorMacroKind kind;
        DescriptorSetType set_type;    ///< 宏归属集合（SetIndex 行其值即宏值；Binding 行为绑定所在集合）
        const char *name;              ///< SetIndex/SetAlias：集合宏名；Binding：绑定宏名
        const char *alias_target;      ///< 仅 SetAlias：目标集合宏名（如 "PER_OBJECT_SET"）；其余 nullptr
        int binding;                   ///< 仅 Binding：绑定号（取自 Binding 枚举）；其余 -1
        const char *comment;           ///< 输出在该宏之前的注释（可含 '\n' 表多行，行内自带 "//"；nullptr 表示无）
    };

    /// 与 descriptor_macros.glsl 一一对应（该 .glsl 为 DescriptorMacroGen 生成物）。
    constexpr const DescriptorBindingMacroSpec kDescriptorBindingMacros[]=
    {
        {DescriptorMacroKind::SetIndex,DescriptorSetType::Scene,    "SCENE_SET",                 nullptr,                                   -1,
            "// ── Descriptor Set 索引 ──"},
        {DescriptorMacroKind::SetIndex,DescriptorSetType::PerObject,"PER_OBJECT_SET",            nullptr,                                   -1, nullptr},
        {DescriptorMacroKind::SetIndex,DescriptorSetType::Material, "MATERIAL_SET",              nullptr,                                   -1, nullptr},

        {DescriptorMacroKind::SetAlias,DescriptorSetType::PerObject,"L2W_SET",                   "PER_OBJECT_SET",                          -1,
            "// ── PerObject set ──"},
        {DescriptorMacroKind::SetAlias,DescriptorSetType::PerObject,"VERTEX_SET",                "PER_OBJECT_SET",                          -1,
            "// ── 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）──\n// s1_position_vec3 / s1_uv / s1_ntb / s1_joint 模块使用"},
        {DescriptorMacroKind::SetAlias,DescriptorSetType::PerObject,"MESH_DRAW_PARAMS_SET",      "PER_OBJECT_SET",                          -1,
            "// mesh per-draw 参数表（IndirectMeshDraw）"},

        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_POSITION_BINDING",   nullptr,   int(PerObjectBinding::VertexPosition),     nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_UV_BINDING",         nullptr,   int(PerObjectBinding::VertexUV),           nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_NTB_BINDING",        nullptr,   int(PerObjectBinding::VertexNTB),          nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_INDEX_BINDING",      nullptr,   int(PerObjectBinding::VertexIndex),        nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_COLOR_BINDING",      nullptr,   int(PerObjectBinding::VertexColor),        nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_LUMINANCE_BINDING",  nullptr,   int(PerObjectBinding::VertexLuminance),    nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_TRANSFORMID_BINDING",nullptr,   int(PerObjectBinding::VertexTransformID),  nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"VERTEX_SIZE_BINDING",       nullptr,   int(PerObjectBinding::VertexSize),         nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"MESH_DRAW_PARAMS_BINDING",  nullptr,   int(PerObjectBinding::MeshDrawParams),
            "// mesh per-draw 参数表（IndirectMeshDraw：mesh shader 经 gl_DrawID 查表）"},

        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"TEXT_CHARINFO_BINDING",     nullptr,   int(PerObjectBinding::TextCharInfo),
            "// ── 文本字符 Quad SSBO（TextCharQuad mesh shader 模式）──"},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"TEXT_CHARSTYLE_BINDING",    nullptr,   int(PerObjectBinding::TextCharStyle),      nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"TEXT_CHARINSTANCE_BINDING", nullptr,   int(PerObjectBinding::TextCharInstance),   nullptr},

        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"L2W_BINDING",               nullptr,   int(PerObjectBinding::L2W),                nullptr},

        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "CAMERA_BINDING",            nullptr,   int(SceneBinding::Camera),
            "// ── Scene set ──"},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "SKY_BINDING",               nullptr,   int(SceneBinding::Sky),                    nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "VIEWPORT_BINDING",          nullptr,   int(SceneBinding::Viewport),               nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "COLOR_PALETTE_BINDING",     nullptr,   int(SceneBinding::ColorPalette),           nullptr},

        {DescriptorMacroKind::SetIndex,DescriptorSetType::Bindless, "BINDLESS_SET",              nullptr,                                   -1, nullptr},
    };
}//namespace hgl::graph
