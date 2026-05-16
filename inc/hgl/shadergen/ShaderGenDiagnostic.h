#pragma once

#include<hgl/common/ShaderStageDef.h>
#include<string>
#include<vector>

namespace hgl::graph
{
enum class ShaderGenSeverity
{
    Info,
    Warning,
    Error
};

enum class ShaderGenErrorCode
{
    None,
    InvalidConfig,
    InvalidShaderStage,
    InvalidDescriptorSemantic,
    DescriptorConflict,
    LayoutNotFinalized,
    SourceGenerationFailed,
    CompileFailed,
    DeviceLimitExceeded,
    InternalError,
    ReflectionMismatch,           ///< G4: SPIR-V 反射结果与声明不一致
    ColorSourceValidationFailed,  ///< G1/G2: ColorSource 结构校验或 binding 分配失败
};

struct ShaderGenDiagnostic
{
    ShaderGenSeverity severity = ShaderGenSeverity::Info;
    ShaderGenErrorCode code = ShaderGenErrorCode::None;
    ShaderStage stage = ShaderStage::Vertex;
    std::string subject;
    std::string message;
};

struct ShaderGenVoid {};

template<typename T>
struct ShaderGenResult
{
    bool success = false;
    T value {};
    std::vector<ShaderGenDiagnostic> diagnostics;

    explicit operator bool() const
    {
        return success;
    }
};

using ShaderGenStatus=ShaderGenResult<ShaderGenVoid>;
}//namespace hgl::graph
