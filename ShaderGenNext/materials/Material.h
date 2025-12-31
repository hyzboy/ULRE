#pragma once

#include "../core/Result.h"
#include "../core/TypeSystem.h"
#include "../compiler/CompilerBackend.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace hgl::shader_next {

/**
 * 材质属性
 */
template<typename T>
class Property {
    std::string name_;
    T value_;
    T default_value_;
    bool is_texture_;
    
public:
    Property(std::string name, T default_val)
        : name_(std::move(name))
        , value_(default_val)
        , default_value_(default_val)
        , is_texture_(false) {}
    
    const std::string& name() const { return name_; }
    const T& value() const { return value_; }
    const T& defaultValue() const { return default_value_; }
    
    void setValue(T val) { value_ = std::move(val); }
    void reset() { value_ = default_value_; }
};

/**
 * Material - 材质基类
 * 
 * 所有材质的基类，定义通用接口
 */
class Material {
protected:
    std::string name_;
    std::string id_;
    
    // 着色器源码（可选，也可以动态生成）
    std::string vertex_source_;
    std::string fragment_source_;
    std::string geometry_source_;
    
public:
    Material(std::string name) : name_(std::move(name)) {
        id_ = generateID();
    }
    
    virtual ~Material() = default;
    
    const std::string& name() const { return name_; }
    const std::string& id() const { return id_; }
    
    // 编译材质到指定 API
    virtual Result<CompiledShader, Error> compile(
        ShaderStage stage,
        const CompileOptions& options
    ) = 0;
    
    // 获取着色器源码
    virtual std::string getShaderSource(ShaderStage stage) const = 0;
    
    // 验证材质配置
    virtual Result<void, Error> validate() const = 0;
    
    // 克隆材质（创建实例）
    virtual std::unique_ptr<Material> clone() const = 0;
    
protected:
    static std::string generateID();
};

/**
 * PBRMaterial - 物理渲染材质
 * 
 * 完整的 PBR 工作流支持
 */
class PBRMaterial : public Material {
public:
    // 基础颜色
    Property<std::array<float, 3>> albedo{"albedo", {1.0f, 1.0f, 1.0f}};
    
    // 金属度
    Property<float> metallic{"metallic", 0.0f};
    
    // 粗糙度
    Property<float> roughness{"roughness", 0.5f};
    
    // 环境光遮蔽
    Property<float> ao{"ao", 1.0f};
    
    // 自发光
    Property<std::array<float, 3>> emissive{"emissive", {0.0f, 0.0f, 0.0f}};
    
    // 纹理（字符串存储纹理名称或路径）
    Property<std::string> albedo_map{"albedoMap", ""};
    Property<std::string> normal_map{"normalMap", ""};
    Property<std::string> metallic_roughness_map{"metallicRoughnessMap", ""};
    Property<std::string> ao_map{"aoMap", ""};
    Property<std::string> emissive_map{"emissiveMap", ""};
    
    // IBL 环境贴图
    Property<std::string> irradiance_map{"irradianceMap", ""};
    Property<std::string> prefilter_map{"prefilterMap", ""};
    Property<std::string> brdf_lut{"brdfLUT", ""};
    
public:
    PBRMaterial() : Material("PBR") {}
    
    Result<CompiledShader, Error> compile(
        ShaderStage stage,
        const CompileOptions& options
    ) override;
    
    std::string getShaderSource(ShaderStage stage) const override;
    
    Result<void, Error> validate() const override;
    
    std::unique_ptr<Material> clone() const override;
    
private:
    // 生成顶点着色器代码
    std::string generateVertexShader() const;
    
    // 生成片段着色器代码
    std::string generateFragmentShader() const;
    
    // PBR 光照计算代码片段
    static std::string getPBRLightingCode();
};

/**
 * UnlitMaterial - 无光照材质
 * 
 * 简单的无光照材质，仅显示颜色或纹理
 */
class UnlitMaterial : public Material {
public:
    Property<std::array<float, 4>> color{"color", {1.0f, 1.0f, 1.0f, 1.0f}};
    Property<std::string> texture{"texture", ""};
    Property<bool> use_vertex_color{"useVertexColor", false};
    
public:
    UnlitMaterial() : Material("Unlit") {}
    
    Result<CompiledShader, Error> compile(
        ShaderStage stage,
        const CompileOptions& options
    ) override;
    
    std::string getShaderSource(ShaderStage stage) const override;
    
    Result<void, Error> validate() const override;
    
    std::unique_ptr<Material> clone() const override;
};

/**
 * CustomMaterial - 自定义材质
 * 
 * 允许用户提供自定义着色器代码
 */
class CustomMaterial : public Material {
    std::unordered_map<ShaderStage, std::string> custom_sources_;
    
public:
    CustomMaterial(std::string name) : Material(std::move(name)) {}
    
    // 设置自定义着色器源码
    void setShaderSource(ShaderStage stage, std::string source) {
        custom_sources_[stage] = std::move(source);
    }
    
    Result<CompiledShader, Error> compile(
        ShaderStage stage,
        const CompileOptions& options
    ) override;
    
    std::string getShaderSource(ShaderStage stage) const override;
    
    Result<void, Error> validate() const override;
    
    std::unique_ptr<Material> clone() const override;
};

/**
 * MaterialBuilder - 材质构建器（流式 API）
 * 
 * 提供声明式的材质创建 API
 */
class MaterialBuilder {
    std::unique_ptr<Material> material_;
    
public:
    MaterialBuilder() = default;
    
    // 设置材质名称
    MaterialBuilder& name(std::string name);
    
    // 选择材质类型
    MaterialBuilder& pbr();
    MaterialBuilder& unlit();
    MaterialBuilder& custom();
    
    // 设置属性
    MaterialBuilder& albedo(float r, float g, float b);
    MaterialBuilder& metallic(float value);
    MaterialBuilder& roughness(float value);
    
    // 设置纹理
    MaterialBuilder& albedoMap(std::string texture);
    MaterialBuilder& normalMap(std::string texture);
    
    // 构建材质
    std::unique_ptr<Material> build();
};

} // namespace hgl::shader_next
