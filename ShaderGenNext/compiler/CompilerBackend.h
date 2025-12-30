#pragma once

#include "../core/Result.h"
#include "../core/TypeSystem.h"
#include <string>
#include <vector>
#include <memory>

namespace hgl::shader_next {

/**
 * 图形 API 枚举
 */
enum class GraphicsAPI {
    Vulkan,
    OpenGL_4_6,
    DirectX12,
    Metal,
    WebGPU
};

/**
 * 着色器阶段
 */
enum class ShaderStage : uint32_t {
    Vertex = 0x01,
    TessControl = 0x02,
    TessEval = 0x04,
    Geometry = 0x08,
    Fragment = 0x10,
    Compute = 0x20,
    Task = 0x40,
    Mesh = 0x80,
    RayGen = 0x100,
    RayClosestHit = 0x200,
    RayMiss = 0x400,
    RayAnyHit = 0x800
};

/**
 * 优化级别
 */
enum class OptimizationLevel {
    None,        // 不优化，快速编译
    Size,        // 优化大小
    Speed,       // 优化速度
    High         // 高级优化
};

/**
 * 特性集
 */
struct FeatureSet {
    bool bindless_textures = false;
    bool ray_tracing = false;
    bool mesh_shaders = false;
    bool variable_rate_shading = false;
    bool int64_support = false;
    bool float64_support = false;
};

/**
 * 编译选项
 */
struct CompileOptions {
    GraphicsAPI target = GraphicsAPI::Vulkan;
    ShaderStage stage = ShaderStage::Vertex;
    OptimizationLevel optimize = OptimizationLevel::High;
    bool debug_info = false;
    bool warnings_as_errors = false;
    FeatureSet features;
    
    // 预处理器定义
    std::unordered_map<std::string, std::string> defines;
    
    // 包含路径
    std::vector<std::string> include_paths;
    
    // 入口点名称（HLSL 需要）
    std::string entry_point = "main";
};

/**
 * 编译结果 - 包含字节码和反射信息
 */
struct CompiledShader {
    std::vector<uint32_t> bytecode;  // SPIR-V 或其他字节码
    ShaderStage stage;
    GraphicsAPI api;
    
    // 反射信息
    struct ReflectionData {
        struct Resource {
            std::string name;
            ShaderType type;
            uint32_t binding;
            uint32_t set;
        };
        
        std::vector<Resource> inputs;
        std::vector<Resource> outputs;
        std::vector<Resource> uniforms;
        std::vector<Resource> samplers;
        std::vector<Resource> storage_buffers;
    } reflection;
    
    // 统计信息
    struct Statistics {
        size_t instruction_count = 0;
        size_t register_count = 0;
        size_t texture_count = 0;
        double compile_time_ms = 0.0;
    } stats;
};

/**
 * 编译器后端抽象接口
 * 
 * 不同的图形 API 实现不同的后端
 */
class CompilerBackend {
public:
    virtual ~CompilerBackend() = default;
    
    // 获取支持的 API
    virtual GraphicsAPI getAPI() const = 0;
    
    // 编译着色器
    virtual Result<CompiledShader, Error> compile(
        const std::string& source,
        const CompileOptions& options
    ) = 0;
    
    // 验证着色器代码
    virtual Result<void, Error> validate(const std::string& source) = 0;
    
    // 反汇编字节码（用于调试）
    virtual std::string disassemble(const CompiledShader& shader) = 0;
    
    // 获取后端信息
    virtual std::string getVersion() const = 0;
    virtual std::string getName() const = 0;
};

/**
 * SPIR-V 编译器后端
 * 
 * 使用 glslang 或 shaderc 编译 GLSL 到 SPIR-V
 */
class SPIRVBackend : public CompilerBackend {
public:
    GraphicsAPI getAPI() const override { return GraphicsAPI::Vulkan; }
    
    Result<CompiledShader, Error> compile(
        const std::string& source,
        const CompileOptions& options
    ) override;
    
    Result<void, Error> validate(const std::string& source) override;
    
    std::string disassemble(const CompiledShader& shader) override;
    
    std::string getVersion() const override;
    std::string getName() const override { return "SPIR-V Compiler"; }
    
private:
    // SPIR-V 特定的辅助函数
    Result<void, Error> performReflection(CompiledShader& shader);
};

/**
 * GLSL 编译器后端
 * 
 * 用于 OpenGL
 */
class GLSLBackend : public CompilerBackend {
public:
    GraphicsAPI getAPI() const override { return GraphicsAPI::OpenGL_4_6; }
    
    Result<CompiledShader, Error> compile(
        const std::string& source,
        const CompileOptions& options
    ) override;
    
    Result<void, Error> validate(const std::string& source) override;
    
    std::string disassemble(const CompiledShader& shader) override;
    
    std::string getVersion() const override;
    std::string getName() const override { return "GLSL Compiler"; }
};

/**
 * HLSL 编译器后端
 * 
 * 使用 DXC 编译 HLSL
 */
class HLSLBackend : public CompilerBackend {
public:
    GraphicsAPI getAPI() const override { return GraphicsAPI::DirectX12; }
    
    Result<CompiledShader, Error> compile(
        const std::string& source,
        const CompileOptions& options
    ) override;
    
    Result<void, Error> validate(const std::string& source) override;
    
    std::string disassemble(const CompiledShader& shader) override;
    
    std::string getVersion() const override;
    std::string getName() const override { return "HLSL Compiler (DXC)"; }
};

/**
 * Metal 编译器后端
 */
class MetalBackend : public CompilerBackend {
public:
    GraphicsAPI getAPI() const override { return GraphicsAPI::Metal; }
    
    Result<CompiledShader, Error> compile(
        const std::string& source,
        const CompileOptions& options
    ) override;
    
    Result<void, Error> validate(const std::string& source) override;
    
    std::string disassemble(const CompiledShader& shader) override;
    
    std::string getVersion() const override;
    std::string getName() const override { return "Metal Compiler"; }
};

/**
 * 编译器工厂 - 自动选择合适的后端
 */
class CompilerFactory {
public:
    // 根据 API 选择后端
    static std::unique_ptr<CompilerBackend> create(GraphicsAPI api);
    
    // 获取默认后端（根据平台自动选择）
    static std::unique_ptr<CompilerBackend> createDefault();
    
    // 检查 API 是否支持
    static bool isSupported(GraphicsAPI api);
    
    // 获取所有支持的 API
    static std::vector<GraphicsAPI> getSupportedAPIs();
};

} // namespace hgl::shader_next
