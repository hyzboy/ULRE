#pragma once

#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/graph/ssbo/SSBOTypes.h>

namespace hgl::graph::mtl
{
    /// 一个着色器资源的全部事实——唯一真源。
    ///
    /// 本表取代原先分散在多处的平行声明（语义枚举 / SSBOType / SBS_* 常量 /
    /// binding 枚举 / GLSL 宏 / Push* 函数各自重复同一行事实）。
    /// **新增资源 = 本表加一行 + 写对应 GLSL 模块**，其余全部查表。
    ///
    /// macro_name 不做机械推导（历史拼写不规则：TRANSFORMID 无下划线），一律显式给出，
    /// 与 ShaderLibrary/common/descriptor_macros.glsl 中的宏名逐字一致。
    struct ShaderResourceSpec
    {
        DescriptorSemantic      semantic;       ///< 语义身份（DescriptorSemantic 枚举）
        DescriptorSemanticLayer layer;          ///< UBO/SSBO/Texture/Sampler
        SSBOType                ssbo_type;      ///< SSBO 类别（非 SSBO 层填 UserDefined）
        DescriptorSetType       set_type;       ///< 归属描述符集合
        int                     binding;        ///< 集内绑定号（ABI：取自 Binding 枚举，禁止手写字面量）
        const char *            name;           ///< GLSL buffer 实例名（原 SBS_*.name）
        const char *            struct_name;    ///< GLSL struct 名（原 SBS_*.struct_name）
        const char *            macro_name;     ///< GLSL binding 宏名（descriptor_macros.glsl）
    };

    /// 顶点数据 SSBO 资源表（Vertex 集，几何 ABI 长期冻结）。
    ///
    /// ⚠️ **行序 = binding 号升序 = descriptor_macros.glsl 的顶点段发射顺序**——
    /// DescriptorMacroGen 依赖此顺序逐字节复刻既有生成物，调整行序会改变 .glsl 文本
    /// （该文本参与着色器内容哈希，变动即全量缓存失效）。
    constexpr const ShaderResourceSpec kShaderResourceSpecs[]=
    {
        {DescriptorSemantic::VertexPosition,    DescriptorSemanticLayer::SSBO,
         SSBOType::VertexPosition,    DescriptorSetType::Vertex, int(VertexBinding::Position),
         "VertexPosition",    "VertexPositionData",    "VERTEX_POSITION_BINDING"},

        {DescriptorSemantic::VertexUV,          DescriptorSemanticLayer::SSBO,
         SSBOType::VertexUV,          DescriptorSetType::Vertex, int(VertexBinding::UV),
         "VertexUV",          "VertexUVData",          "VERTEX_UV_BINDING"},

        {DescriptorSemantic::VertexNTB,         DescriptorSemanticLayer::SSBO,
         SSBOType::VertexNTB,         DescriptorSetType::Vertex, int(VertexBinding::NTB),
         "VertexNTB",         "VertexNTBData",         "VERTEX_NTB_BINDING"},

        {DescriptorSemantic::VertexIndex,       DescriptorSemanticLayer::SSBO,
         SSBOType::VertexIndex,       DescriptorSetType::Vertex, int(VertexBinding::Index),
         "VertexIndex",       "VertexIndexData",       "VERTEX_INDEX_BINDING"},

        {DescriptorSemantic::VertexColor,       DescriptorSemanticLayer::SSBO,
         SSBOType::VertexColor,       DescriptorSetType::Vertex, int(VertexBinding::Color),
         "VertexColor",       "VertexColorData",       "VERTEX_COLOR_BINDING"},

        {DescriptorSemantic::VertexLuminance,   DescriptorSemanticLayer::SSBO,
         SSBOType::VertexLuminance,   DescriptorSetType::Vertex, int(VertexBinding::Luminance),
         "VertexLuminance",   "VertexLuminanceData",   "VERTEX_LUMINANCE_BINDING"},

        {DescriptorSemantic::VertexTransformID, DescriptorSemanticLayer::SSBO,
         SSBOType::VertexTransformID, DescriptorSetType::Vertex, int(VertexBinding::TransformID),
         "VertexTransformID", "VertexTransformIDData", "VERTEX_TRANSFORMID_BINDING"},

        {DescriptorSemantic::VertexSize,        DescriptorSemanticLayer::SSBO,
         SSBOType::VertexSize,        DescriptorSetType::Vertex, int(VertexBinding::Size),
         "VertexSize",        "VertexSizeData",        "VERTEX_SIZE_BINDING"},
    };

    constexpr const int kShaderResourceSpecCount=
        int(sizeof(kShaderResourceSpecs)/sizeof(kShaderResourceSpecs[0]));

    /// 按语义查规范行；未声明语义返回 nullptr（调用方须显式处理，不做静默默认）。
    constexpr const ShaderResourceSpec *FindShaderResourceSpec(const DescriptorSemantic semantic) noexcept
    {
        for(const ShaderResourceSpec &spec:kShaderResourceSpecs)
            if(spec.semantic==semantic)
                return &spec;

        return nullptr;
    }
}//namespace hgl::graph::mtl
