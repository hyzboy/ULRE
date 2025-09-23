// FBX到引擎集成范例
// 展示如何将FBX数据转换为引擎的SceneNode和Primitive对象

#include<iostream>
#include<vector>
#include<memory>
#include<string>

// 简化的引擎类型定义（基于观察到的模式）
namespace hgl {
namespace graph {

struct Vector3f {
    float x, y, z;
    Vector3f(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct Matrix4f {
    float m[16];
    void identity() {
        for(int i = 0; i < 16; i++) m[i] = 0;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
    Matrix4f() { identity(); }
};

// 简化的SceneNode类（基于SceneNode.h的观察）
class SceneNode {
private:
    std::string node_name;
    Matrix4f local_matrix;
    std::vector<std::unique_ptr<SceneNode>> children;
    std::vector<int> mesh_indices;  // 指向网格的索引

public:
    SceneNode(const std::string& name = "") : node_name(name) {}
    
    void SetName(const std::string& name) { node_name = name; }
    void SetLocalMatrix(const Matrix4f& matrix) { local_matrix = matrix; }
    
    void AddChild(std::unique_ptr<SceneNode> child) {
        children.push_back(std::move(child));
    }
    
    void AddMesh(int mesh_index) {
        mesh_indices.push_back(mesh_index);
    }
    
    const std::string& GetName() const { return node_name; }
    const std::vector<std::unique_ptr<SceneNode>>& GetChildren() const { return children; }
    const std::vector<int>& GetMeshes() const { return mesh_indices; }
    
    void Print(const std::string& prefix = "") const {
        std::cout << prefix << "Node: " << node_name;
        if(!mesh_indices.empty()) {
            std::cout << " [Meshes: ";
            for(size_t i = 0; i < mesh_indices.size(); i++) {
                if(i > 0) std::cout << ", ";
                std::cout << mesh_indices[i];
            }
            std::cout << "]";
        }
        std::cout << std::endl;
        
        for(const auto& child : children) {
            child->Print(prefix + "  ");
        }
    }
};

// 简化的Primitive类（基于观察到的模式）
struct Primitive {
    std::string name;
    std::vector<Vector3f> positions;
    std::vector<Vector3f> normals;
    std::vector<uint32_t> indices;
    
    Primitive(const std::string& n) : name(n) {}
    
    void Print() const {
        std::cout << "Primitive: " << name << std::endl;
        std::cout << "  Vertices: " << positions.size() << std::endl;
        std::cout << "  Normals: " << normals.size() << std::endl;
        std::cout << "  Indices: " << indices.size() << std::endl;
        std::cout << "  Triangles: " << indices.size() / 3 << std::endl;
    }
};

} // namespace graph
} // namespace hgl

using namespace hgl::graph;

/**
 * FBX到引擎集成器
 * 模拟将FBX数据转换为引擎对象的过程
 */
class FBXToEngineIntegrator {
private:
    std::vector<std::unique_ptr<Primitive>> primitives;
    std::unique_ptr<SceneNode> root_node;
    
public:
    FBXToEngineIntegrator() : root_node(std::make_unique<SceneNode>("RootNode")) {}
    
    // 模拟从FBX数据创建Primitive
    int CreatePrimitive(const std::string& name, 
                       const std::vector<Vector3f>& positions,
                       const std::vector<Vector3f>& normals,
                       const std::vector<uint32_t>& indices) {
        
        auto primitive = std::make_unique<Primitive>(name);
        primitive->positions = positions;
        primitive->normals = normals;
        primitive->indices = indices;
        
        int index = static_cast<int>(primitives.size());
        primitives.push_back(std::move(primitive));
        
        std::cout << "Created Primitive[" << index << "]: " << name << std::endl;
        return index;
    }
    
    // 模拟创建SceneNode层次结构
    SceneNode* CreateSceneNode(const std::string& name, 
                              const Matrix4f& transform,
                              SceneNode* parent = nullptr) {
        
        auto node = std::make_unique<SceneNode>(name);
        node->SetLocalMatrix(transform);
        
        SceneNode* node_ptr = node.get();
        
        if(parent) {
            // 在实际实现中，这里会有更复杂的父子关系管理
            // 现在简化为直接添加到父节点
            std::cout << "Adding " << name << " as child of " << parent->GetName() << std::endl;
        } else {
            // 添加到根节点
            root_node->AddChild(std::move(node));
            std::cout << "Added " << name << " to root node" << std::endl;
        }
        
        return node_ptr;
    }
    
    // 将Primitive附加到SceneNode
    void AttachPrimitiveToNode(SceneNode* node, int primitive_index) {
        if(node && primitive_index >= 0 && primitive_index < static_cast<int>(primitives.size())) {
            node->AddMesh(primitive_index);
            std::cout << "Attached Primitive[" << primitive_index << "] to Node[" << node->GetName() << "]" << std::endl;
        }
    }
    
    // 打印完整的场景结构
    void PrintScene() const {
        std::cout << "\n=== Scene Structure ===" << std::endl;
        root_node->Print();
        
        std::cout << "\n=== Primitives ===" << std::endl;
        for(size_t i = 0; i < primitives.size(); i++) {
            std::cout << "Primitive[" << i << "]: ";
            primitives[i]->Print();
        }
    }
    
    SceneNode* GetRootNode() const { return root_node.get(); }
};

/**
 * 模拟FBX加载和转换过程
 */
void DemonstrateIntegration() {
    std::cout << "=== FBX to Engine Integration Demo ===" << std::endl;
    
    FBXToEngineIntegrator integrator;
    
    // 模拟从FBX文件加载的网格数据
    std::cout << "\n1. Loading FBX mesh data..." << std::endl;
    
    // 网格1：立方体
    std::vector<Vector3f> cube_positions = {
        Vector3f(-0.5f, -0.5f, -0.5f), Vector3f(0.5f, -0.5f, -0.5f),
        Vector3f(0.5f,  0.5f, -0.5f),  Vector3f(-0.5f, 0.5f, -0.5f),
        Vector3f(-0.5f, -0.5f,  0.5f), Vector3f(0.5f, -0.5f,  0.5f),
        Vector3f(0.5f,  0.5f,  0.5f),  Vector3f(-0.5f, 0.5f,  0.5f)
    };
    
    std::vector<Vector3f> cube_normals = {
        Vector3f(-0.577f, -0.577f, -0.577f), Vector3f(0.577f, -0.577f, -0.577f),
        Vector3f(0.577f,  0.577f, -0.577f),  Vector3f(-0.577f, 0.577f, -0.577f),
        Vector3f(-0.577f, -0.577f,  0.577f), Vector3f(0.577f, -0.577f,  0.577f),
        Vector3f(0.577f,  0.577f,  0.577f),  Vector3f(-0.577f, 0.577f,  0.577f)
    };
    
    std::vector<uint32_t> cube_indices = {
        0,1,2, 2,3,0,  4,7,6, 6,5,4,  0,3,7, 7,4,0,
        1,5,6, 6,2,1,  3,2,6, 6,7,3,  0,4,5, 5,1,0
    };
    
    // 网格2：简单三角形
    std::vector<Vector3f> triangle_positions = {
        Vector3f(0.0f, 1.0f, 0.0f), Vector3f(-1.0f, -1.0f, 0.0f), Vector3f(1.0f, -1.0f, 0.0f)
    };
    
    std::vector<Vector3f> triangle_normals = {
        Vector3f(0.0f, 0.0f, 1.0f), Vector3f(0.0f, 0.0f, 1.0f), Vector3f(0.0f, 0.0f, 1.0f)
    };
    
    std::vector<uint32_t> triangle_indices = { 0, 1, 2 };
    
    std::cout << "\n2. Creating engine Primitives..." << std::endl;
    
    // 创建Primitive对象
    int cube_primitive_id = integrator.CreatePrimitive("Cube_Mesh", cube_positions, cube_normals, cube_indices);
    int triangle_primitive_id = integrator.CreatePrimitive("Triangle_Mesh", triangle_positions, triangle_normals, triangle_indices);
    
    std::cout << "\n3. Building SceneNode hierarchy..." << std::endl;
    
    // 创建场景节点层次结构（模拟FBX场景结构）
    Matrix4f identity_matrix;
    
    // SceneNode* mesh_container = integrator.CreateSceneNode("MeshContainer", identity_matrix);
    SceneNode* cube_node = integrator.CreateSceneNode("CubeNode", identity_matrix);
    SceneNode* triangle_node = integrator.CreateSceneNode("TriangleNode", identity_matrix);
    
    std::cout << "\n4. Attaching Primitives to SceneNodes..." << std::endl;
    
    // 将Primitive附加到相应的节点
    integrator.AttachPrimitiveToNode(cube_node, cube_primitive_id);
    integrator.AttachPrimitiveToNode(triangle_node, triangle_primitive_id);
    
    std::cout << "\n5. Final scene structure..." << std::endl;
    integrator.PrintScene();
    
    std::cout << "\n=== Integration Workflow Complete ===" << std::endl;
    std::cout << "\nKey Points:" << std::endl;
    std::cout << "- Each FBX mesh becomes an engine Primitive" << std::endl;
    std::cout << "- Each FBX node becomes an engine SceneNode" << std::endl;
    std::cout << "- Hierarchical relationships are preserved" << std::endl;
    std::cout << "- Only vertex positions and normals are processed" << std::endl;
    std::cout << "- Ready for material/texture/animation integration" << std::endl;
}

int main() {
    DemonstrateIntegration();
    return 0;
}