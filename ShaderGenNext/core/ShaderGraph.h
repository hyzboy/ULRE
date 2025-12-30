#pragma once

#include "TypeSystem.h"
#include "Result.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace hgl::shader_next {

/**
 * ShaderNode - 着色器图节点基类
 * 
 * 每个节点代表一个着色器操作（纹理采样、数学运算、光照计算等）
 */
class ShaderNode {
protected:
    std::string name_;
    std::string id_;
    
    struct Pin {
        std::string name;
        ShaderType type;
        bool is_input;
        void* default_value;
    };
    
    std::vector<Pin> inputs_;
    std::vector<Pin> outputs_;
    
public:
    ShaderNode(std::string name, std::string id = "")
        : name_(std::move(name)), id_(id.empty() ? generateID() : std::move(id)) {}
    
    virtual ~ShaderNode() = default;
    
    // 节点信息
    const std::string& name() const { return name_; }
    const std::string& id() const { return id_; }
    
    // 输入输出管理
    void addInput(std::string name, ShaderType type, void* default_val = nullptr);
    void addOutput(std::string name, ShaderType type);
    
    const std::vector<Pin>& inputs() const { return inputs_; }
    const std::vector<Pin>& outputs() const { return outputs_; }
    
    // 代码生成
    virtual std::string generateCode() const = 0;
    
    // 验证节点配置
    virtual Result<void, Error> validate() const;
    
private:
    static std::string generateID();
};

/**
 * 常用节点实现
 */

// 纹理采样节点
class TextureSampleNode : public ShaderNode {
    std::string texture_name_;
    
public:
    TextureSampleNode(std::string texture_name);
    std::string generateCode() const override;
};

// 数学运算节点
class MathNode : public ShaderNode {
public:
    enum class Operation {
        Add, Sub, Mul, Div,
        Dot, Cross, Normalize, Length,
        Min, Max, Clamp, Mix,
        Sin, Cos, Tan, Pow, Sqrt
    };
    
private:
    Operation op_;
    
public:
    MathNode(Operation op);
    std::string generateCode() const override;
};

// PBR 光照节点
class PBRLightingNode : public ShaderNode {
public:
    PBRLightingNode();
    std::string generateCode() const override;
};

// 常量节点
template<typename T>
class ConstantNode : public ShaderNode {
    T value_;
    
public:
    ConstantNode(T value) : ShaderNode("Constant"), value_(std::move(value)) {
        static_assert(IsShaderType<T>::value, "T must be a valid shader type");
        addOutput("value", IsShaderType<T>::value);
    }
    
    std::string generateCode() const override {
        // 生成常量值的代码
        return "const " + IsShaderType<T>::value.name() + " " + id() + " = ...;";
    }
};

// 输出节点
class OutputNode : public ShaderNode {
public:
    OutputNode();
    std::string generateCode() const override;
};

// 输入节点
class InputNode : public ShaderNode {
public:
    InputNode(std::string name, ShaderType type);
    std::string generateCode() const override;
};

/**
 * 节点连接
 */
struct Connection {
    std::string from_node;
    std::string from_output;
    std::string to_node;
    std::string to_input;
    
    bool isValid() const;
};

/**
 * ShaderGraph - 着色器图
 * 
 * 管理节点和连接，生成最终的着色器代码
 */
class ShaderGraph {
    std::string name_;
    std::vector<std::unique_ptr<ShaderNode>> nodes_;
    std::vector<Connection> connections_;
    
public:
    explicit ShaderGraph(std::string name) : name_(std::move(name)) {}
    
    static std::shared_ptr<ShaderGraph> create(std::string name) {
        return std::make_shared<ShaderGraph>(std::move(name));
    }
    
    // 节点管理
    template<typename NodeType, typename... Args>
    NodeType* addNode(Args&&... args) {
        auto node = std::make_unique<NodeType>(std::forward<Args>(args)...);
        auto* ptr = node.get();
        nodes_.push_back(std::move(node));
        return ptr;
    }
    
    ShaderNode* findNode(const std::string& id);
    bool removeNode(const std::string& id);
    
    // 连接管理
    Result<void, Error> connect(
        const std::string& from_node, const std::string& from_output,
        const std::string& to_node, const std::string& to_input
    );
    
    bool disconnect(
        const std::string& from_node, const std::string& from_output,
        const std::string& to_node, const std::string& to_input
    );
    
    // 验证图的完整性
    Result<void, Error> validate() const;
    
    // 拓扑排序（确定节点执行顺序）
    Result<std::vector<ShaderNode*>, Error> topologicalSort() const;
    
    // 生成代码
    Result<std::string, Error> generateCode() const;
    
    // 编译（将在 Compiler 模块实现）
    // Result<CompiledShader, Error> compile(const CompileOptions& options) const;
    
    // 序列化
    std::string serialize() const;
    static Result<ShaderGraph, Error> deserialize(const std::string& data);
};

/**
 * 节点工厂 - 用于注册和创建自定义节点
 */
class NodeFactory {
    using NodeCreator = std::function<std::unique_ptr<ShaderNode>()>;
    std::unordered_map<std::string, NodeCreator> creators_;
    
public:
    static NodeFactory& instance();
    
    // 注册节点类型
    template<typename NodeType>
    void registerNode(const std::string& type_name) {
        creators_[type_name] = []() {
            return std::make_unique<NodeType>();
        };
    }
    
    // 创建节点
    std::unique_ptr<ShaderNode> create(const std::string& type_name);
    
    // 获取所有注册的节点类型
    std::vector<std::string> getRegisteredTypes() const;
};

// 便利宏，用于注册节点
#define REGISTER_SHADER_NODE(NodeType, TypeName) \
    namespace { \
        struct NodeType##Registrar { \
            NodeType##Registrar() { \
                NodeFactory::instance().registerNode<NodeType>(TypeName); \
            } \
        }; \
        static NodeType##Registrar g_##NodeType##_registrar; \
    }

} // namespace hgl::shader_next
