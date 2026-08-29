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

        MeshDrawParams   = 13,  ///< mesh per-draw 参数表 SSBO（indirect 合批查表）

        // CharQuad 文本字符 SSBO（TextCharQuad mesh shader 模式）
        TextCharInfo     = 14,  ///< 字符信息 SSBO
        TextCharStyle    = 15,  ///< 字符样式 SSBO
        TextCharInstance = 16,  ///< 字符实例 SSBO
    };

    /// Vertex 集（Set 4）绑定号——顶点数据 SSBO 专用集（Phase 5 自 PerObject 迁出）。
    /// 几何 ABI，长期冻结；与每批更新的 PerObject 集（MeshDrawParams 等）演化解耦，
    /// 为 meshlet/nanite 留出演化空间。binding 连号，新顶点流按序追加。
    enum class VertexBinding : int
    {
        Position    = 0,    ///< 顶点位置 SSBO
        UV          = 1,    ///< 顶点 UV SSBO
        NTB         = 2,    ///< 顶点 NTB SSBO
        Index       = 3,    ///< 顶点索引 SSBO
        Color       = 4,    ///< 顶点颜色 SSBO
        Luminance   = 5,    ///< 顶点亮度 SSBO
        TransformID = 6,    ///< 顶点 TransformID SSBO（调色板变换索引）
        Size        = 7,    ///< 顶点 Size/宽度 SSBO（Line width）
    };

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

    static_assert(int(VertexBinding::Position)==0
               && int(VertexBinding::Size)==7,
                  "Vertex binding ABI changed");

    /// ── 兼容别名：既有调用点继续使用 kXxx 常量名，数值真源已上收至上述枚举 ──
    constexpr const int kSceneBindingCamera       = int(SceneBinding::Camera);        ///< 相机 UBO
    constexpr const int kSceneBindingSky          = int(SceneBinding::Sky);           ///< 天空/太阳光 UBO
    constexpr const int kSceneBindingViewport     = int(SceneBinding::Viewport);      ///< 视口 UBO
    constexpr const int kSceneBindingColorPalette = int(SceneBinding::ColorPalette);  ///< 顶点调色板 UBO

    constexpr const int kPerObjectBindingL2W               = int(PerObjectBinding::L2W);              ///< per-draw 变换数据
    constexpr const int kPerObjectBindingL2WIndex          = int(PerObjectBinding::L2WIndex);         ///< 实例 → l2w 行索引表
    constexpr const int kPerObjectBindingPrivateDataIndex  = int(PerObjectBinding::PrivateDataIndex); ///< 实例 → 材质私有数据行索引表
    constexpr const int kPerObjectBindingMeshDrawParams    = int(PerObjectBinding::MeshDrawParams);   ///< mesh per-draw 参数表 SSBO
    constexpr const int kPerObjectBindingTextCharInfo      = int(PerObjectBinding::TextCharInfo);     ///< 字符信息 SSBO
    constexpr const int kPerObjectBindingTextCharStyle     = int(PerObjectBinding::TextCharStyle);    ///< 字符样式 SSBO
    constexpr const int kPerObjectBindingTextCharInstance  = int(PerObjectBinding::TextCharInstance); ///< 字符实例 SSBO

    enum class DescriptorSetType:int
    {
        Unknow=-1,

        Scene=0,        ///< 全局 UBO 集（camera/sky/viewport/color_palette），所有材质共用，一帧写/绑一次
        PerObject,      ///< per-object/per-draw SSBO 集（l2w/l2w_index/material_private_data_index/mesh_draw_params）
        Material,       ///< per-material 描述符集（mtl 数据槽/索引表）
        Bindless,       ///< 全局 Bindless 纹理数组集合（Set 3），一帧绑一次
        Vertex,         ///< 顶点数据 SSBO 集（Set 4，Phase 5 自 PerObject 迁出）——几何 ABI，长期冻结

        ENUM_CLASS_RANGE(Scene,Vertex)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    constexpr const char *DescriptSetTypeName[]=
    {
        "Scene",
        "PerObject",
        "Material",
        "Bindless",
        "Vertex"
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
    ///
    /// blank_before/blank_after_comment 用于逐字节复刻现行手写文件的空行布局
    ///（该文件文本经模块系统拼入 FinalGLSL 参与内容哈希，格式必须稳定）。
    struct DescriptorBindingMacroSpec
    {
        DescriptorMacroKind kind;
        DescriptorSetType set_type;    ///< 宏归属集合（SetIndex 行其值即宏值；Binding 行为绑定所在集合）
        const char *name;              ///< SetIndex/SetAlias：集合宏名；Binding：绑定宏名
        const char *alias_target;      ///< 仅 SetAlias：目标集合宏名（如 "PER_OBJECT_SET"）；其余 nullptr
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
        {DescriptorMacroKind::SetIndex,DescriptorSetType::PerObject,"PER_OBJECT_SET",            nullptr,                                   -1, nullptr},
        {DescriptorMacroKind::SetIndex,DescriptorSetType::Material, "MATERIAL_SET",              nullptr,                                   -1, nullptr},

        {DescriptorMacroKind::SetAlias,DescriptorSetType::PerObject,"L2W_SET",                   "PER_OBJECT_SET",                          -1,
            "// ── PerObject set ──",                                  true, true},
        {DescriptorMacroKind::SetIndex,DescriptorSetType::Vertex,   "VERTEX_SET",                nullptr,                                   -1,
            "// ── 顶点数据 SSBO（Vertex 集，Phase 5 自 PerObject 迁出）──\n// s1_position_vec3 / s1_uv / s1_ntb / s1_joint 模块使用"},
        {DescriptorMacroKind::SetAlias,DescriptorSetType::PerObject,"MESH_DRAW_PARAMS_SET",      "PER_OBJECT_SET",                          -1,
            "// mesh per-draw 参数表（IndirectMeshDraw）"},

        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_POSITION_BINDING",   nullptr,   int(VertexBinding::Position),     nullptr, false},
        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_UV_BINDING",         nullptr,   int(VertexBinding::UV),           nullptr, false},
        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_NTB_BINDING",        nullptr,   int(VertexBinding::NTB),          nullptr, false},
        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_INDEX_BINDING",      nullptr,   int(VertexBinding::Index),        nullptr, false},
        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_COLOR_BINDING",      nullptr,   int(VertexBinding::Color),        nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_LUMINANCE_BINDING",  nullptr,   int(VertexBinding::Luminance),    nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_TRANSFORMID_BINDING",nullptr,   int(VertexBinding::TransformID),  nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Vertex,   "VERTEX_SIZE_BINDING",       nullptr,   int(VertexBinding::Size),         nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"MESH_DRAW_PARAMS_BINDING",  nullptr,   int(PerObjectBinding::MeshDrawParams),
            "// mesh per-draw 参数表（IndirectMeshDraw：mesh shader 经 gl_DrawID 查表）"},

        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"TEXT_CHARINFO_BINDING",     nullptr,   int(PerObjectBinding::TextCharInfo),
            "// ── 文本字符 Quad SSBO（TextCharQuad mesh shader 模式）──"},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"TEXT_CHARSTYLE_BINDING",    nullptr,   int(PerObjectBinding::TextCharStyle),      nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"TEXT_CHARINSTANCE_BINDING", nullptr,   int(PerObjectBinding::TextCharInstance),   nullptr},

        {DescriptorMacroKind::Binding, DescriptorSetType::PerObject,"L2W_BINDING",               nullptr,   int(PerObjectBinding::L2W),                nullptr},

        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "CAMERA_BINDING",            nullptr,   int(SceneBinding::Camera),
            "// ── Scene set ──",                                      true, true},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "SKY_BINDING",               nullptr,   int(SceneBinding::Sky),                    nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "VIEWPORT_BINDING",          nullptr,   int(SceneBinding::Viewport),               nullptr},
        {DescriptorMacroKind::Binding, DescriptorSetType::Scene,    "COLOR_PALETTE_BINDING",     nullptr,   int(SceneBinding::ColorPalette),           nullptr},

        {DescriptorMacroKind::SetIndex,DescriptorSetType::Bindless, "BINDLESS_SET",              nullptr,                                   -1, nullptr},
    };
}//namespace hgl::graph
