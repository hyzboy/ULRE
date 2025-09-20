// GLTF Model Loader Test Example
// 该范例演示如何读取GLTF模型并将其场景层次结构映射到引擎的SceneNode系统
// 暂时只处理网格模型，不考虑骷髅动画、贴图、材质、动画、灯光等
// 但预留了结构供未来实现
//
// 架构说明：
// 1. GLTF节点 -> 引擎SceneNode：每个GLTF节点对应一个SceneNode，保持层次结构
// 2. GLTF Primitive -> 引擎Primitive：每个GLTF primitive创建独立的Primitive/Mesh
// 3. GLTF Mesh -> 特殊SceneNode：每个GLTF mesh封装成一个特殊的SceneNode，包含多个primitive子节点
// 4. 未来扩展点：材质系统、纹理加载、动画系统、灯光系统

#include<hgl/WorkManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/color/Color.h>
#include<hgl/log/LogInfo.h>
#include<vector>
#include<string>
#include<memory>
#include<map>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;

// 简单的GLTF数据结构定义
struct GLTFAccessor {
    uint32_t bufferView = 0;
    uint32_t byteOffset = 0;
    uint32_t componentType = 0;
    uint32_t count = 0;
    std::string type;
    std::vector<float> min, max;
};

struct GLTFBufferView {
    uint32_t buffer = 0;
    uint32_t byteOffset = 0;
    uint32_t byteLength = 0;
    uint32_t byteStride = 0;
    uint32_t target = 0;
};

struct GLTFBuffer {
    uint32_t byteLength = 0;
    std::string uri;
    std::vector<uint8_t> data;
};

struct GLTFPrimitive {
    std::map<std::string, uint32_t> attributes;  // POSITION, NORMAL, TEXCOORD_0, etc.
    uint32_t indices = 0;
    uint32_t material = 0;
    uint32_t mode = 4; // TRIANGLES
};

struct GLTFMesh {
    std::string name;
    std::vector<GLTFPrimitive> primitives;
};

struct GLTFNode {
    std::string name;
    std::vector<uint32_t> children;
    uint32_t mesh = UINT32_MAX;
    std::vector<float> matrix = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; // identity matrix
    std::vector<float> translation, rotation, scale;
};

struct GLTFScene {
    std::string name;
    std::vector<uint32_t> nodes;
};

struct GLTFMaterial {
    std::string name;
    
    // PBR Metal/Roughness材质模型（未来实现）
    struct {
        std::vector<float> baseColorFactor = {1,1,1,1};
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        uint32_t baseColorTexture = UINT32_MAX;  // 纹理索引，未来实现
        uint32_t metallicRoughnessTexture = UINT32_MAX;
    } pbrMetallicRoughness;
    
    // 其他材质属性（未来实现）
    uint32_t normalTexture = UINT32_MAX;        // 法线贴图
    uint32_t occlusionTexture = UINT32_MAX;     // 遮挡贴图
    uint32_t emissiveTexture = UINT32_MAX;      // 自发光贴图
    std::vector<float> emissiveFactor = {0,0,0}; // 自发光因子
    std::string alphaMode = "OPAQUE";           // 透明模式
    float alphaCutoff = 0.5f;                   // 透明度裁剪值
    bool doubleSided = false;                   // 双面渲染
};

struct GLTFAsset {
    std::string version = "2.0";
    std::string generator;
};

// 纹理相关结构（未来实现）
struct GLTFTexture {
    uint32_t sampler = UINT32_MAX;
    uint32_t source = UINT32_MAX;   // 图像索引
    std::string name;
};

struct GLTFImage {
    std::string uri;
    std::string mimeType;
    uint32_t bufferView = UINT32_MAX;
    std::string name;
};

struct GLTFSampler {
    uint32_t magFilter = 9729;      // LINEAR
    uint32_t minFilter = 9987;      // LINEAR_MIPMAP_LINEAR  
    uint32_t wrapS = 10497;         // REPEAT
    uint32_t wrapT = 10497;         // REPEAT
    std::string name;
};

// 动画相关结构（未来实现）
struct GLTFAnimationChannel {
    uint32_t sampler = 0;
    struct {
        uint32_t node = 0;
        std::string path;  // "translation", "rotation", "scale", "weights"
    } target;
};

struct GLTFAnimationSampler {
    uint32_t input = 0;   // 时间戳accessor
    uint32_t output = 0;  // 值accessor
    std::string interpolation = "LINEAR";  // "LINEAR", "STEP", "CUBICSPLINE"
};

struct GLTFAnimation {
    std::string name;
    std::vector<GLTFAnimationChannel> channels;
    std::vector<GLTFAnimationSampler> samplers;
};

// 骨骼动画相关结构（未来实现）
struct GLTFSkin {
    std::string name;
    uint32_t inverseBindMatrices = UINT32_MAX;  // accessor索引
    std::vector<uint32_t> joints;               // 节点索引数组
    uint32_t skeleton = UINT32_MAX;             // 根节点索引
};

// GLTF文件结构
struct GLTFDocument {
    GLTFAsset asset;
    std::vector<GLTFAccessor> accessors;
    std::vector<GLTFBufferView> bufferViews;
    std::vector<GLTFBuffer> buffers;
    std::vector<GLTFMesh> meshes;
    std::vector<GLTFNode> nodes;
    std::vector<GLTFScene> scenes;
    std::vector<GLTFMaterial> materials;
    
    // 未来实现的扩展
    std::vector<GLTFTexture> textures;
    std::vector<GLTFImage> images;
    std::vector<GLTFSampler> samplers;
    std::vector<GLTFAnimation> animations;
    std::vector<GLTFSkin> skins;
    
    uint32_t scene = 0; // default scene index
};

// 简单的JSON解析器（仅支持GLTF需要的基本功能）
class SimpleJSONParser {
private:
    std::string data;
    size_t pos = 0;
    
    void skipWhitespace() {
        while (pos < data.length() && (data[pos] == ' ' || data[pos] == '\t' || 
               data[pos] == '\n' || data[pos] == '\r')) {
            pos++;
        }
    }
    
    std::string parseString() {
        if (data[pos] != '"') return "";
        pos++; // skip opening quote
        
        std::string result;
        while (pos < data.length() && data[pos] != '"') {
            if (data[pos] == '\\' && pos + 1 < data.length()) {
                pos++; // skip escape char
                switch (data[pos]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '\\': result += '\\'; break;
                    case '"': result += '"'; break;
                    default: result += data[pos]; break;
                }
            } else {
                result += data[pos];
            }
            pos++;
        }
        
        if (pos < data.length()) pos++; // skip closing quote
        return result;
    }
    
    float parseNumber() {
        size_t start = pos;
        if (data[pos] == '-') pos++;
        while (pos < data.length() && (std::isdigit(data[pos]) || data[pos] == '.')) {
            pos++;
        }
        if (pos < data.length() && (data[pos] == 'e' || data[pos] == 'E')) {
            pos++;
            if (data[pos] == '+' || data[pos] == '-') pos++;
            while (pos < data.length() && std::isdigit(data[pos])) {
                pos++;
            }
        }
        return std::stof(data.substr(start, pos - start));
    }
    
    uint32_t parseUInt() {
        size_t start = pos;
        while (pos < data.length() && std::isdigit(data[pos])) {
            pos++;
        }
        return std::stoul(data.substr(start, pos - start));
    }
    
public:
    SimpleJSONParser(const std::string& json) : data(json) {}
    
    bool parseGLTF(GLTFDocument& doc) {
        // 这里应该实现完整的JSON解析，为了简化示例，我们使用硬编码的测试数据
        // 实际项目中应该使用jsoncpp或类似的JSON库
        
        // 创建一个简单的三角形网格作为测试数据
        GLTFBuffer buffer;
        buffer.byteLength = 36 * sizeof(float); // 3 vertices * 3 components * 4 bytes
        buffer.data.resize(buffer.byteLength);
        
        // 三角形顶点数据
        float vertices[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f
        };
        
        memcpy(buffer.data.data(), vertices, sizeof(vertices));
        doc.buffers.push_back(buffer);
        
        GLTFBufferView bufferView;
        bufferView.buffer = 0;
        bufferView.byteOffset = 0;
        bufferView.byteLength = sizeof(vertices);
        bufferView.target = 34962; // ARRAY_BUFFER
        doc.bufferViews.push_back(bufferView);
        
        GLTFAccessor accessor;
        accessor.bufferView = 0;
        accessor.byteOffset = 0;
        accessor.componentType = 5126; // FLOAT
        accessor.count = 3;
        accessor.type = "VEC3";
        accessor.min = {-0.5f, -0.5f, 0.0f};
        accessor.max = {0.5f, 0.5f, 0.0f};
        doc.accessors.push_back(accessor);
        
        GLTFPrimitive primitive;
        primitive.attributes["POSITION"] = 0;
        primitive.mode = 4; // TRIANGLES
        
        GLTFMesh mesh;
        mesh.name = "Triangle";
        mesh.primitives.push_back(primitive);
        doc.meshes.push_back(mesh);
        
        GLTFNode node;
        node.name = "TriangleNode";
        node.mesh = 0;
        doc.nodes.push_back(node);
        
        GLTFScene scene;
        scene.name = "Scene";
        scene.nodes.push_back(0);
        doc.scenes.push_back(scene);
        
        doc.scene = 0;
        
        return true;
    }
};

// GLTF加载器类
class GLTFLoader {
private:
    GLTFDocument document;
    RenderFramework* framework;
    
    // 材质和渲染管线
    Material* default_material = nullptr;
    MaterialInstance* material_instance = nullptr;
    Pipeline* pipeline = nullptr;
    
    // 存储创建的场景节点，供SceneNode引用
    std::vector<SceneNode*> created_nodes;
    
    // TODO: 未来扩展
    // std::vector<Texture*> loaded_textures;          // 纹理缓存
    // std::vector<MaterialInstance*> loaded_materials; // 材质实例缓存
    // std::vector<AnimationComponent*> animations;     // 动画组件
    
public:
    GLTFLoader(RenderFramework* fw) : framework(fw) {}
    
    ~GLTFLoader() {
        // 清理资源
        for (auto node : created_nodes) {
            delete node;
        }
    }
    
    bool LoadTestData() {
        // 使用内置测试数据创建一个简单的三角形
        SimpleJSONParser parser("");
        return parser.parseGLTF(document);
    }
    
    bool InitializeRenderResources() {
        // 创建默认材质
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);
        
        mtl::MaterialCreateInfo* mci = mtl::CreateGizmo3D(framework->GetDevAttr(), &cfg);
        if (!mci) {
            return false;
        }
        
        default_material = framework->CreateMaterial("GLTF_Default", mci);
        if (!default_material) {
            return false;
        }
        
        Color4f color = GetColor4f(COLOR::White);
        material_instance = framework->CreateMaterialInstance(default_material, (VIL*)nullptr, &color);
        if (!material_instance) {
            return false;
        }
        
        pipeline = framework->CreatePipeline(material_instance, InlinePipeline::Solid3D);
        
        return pipeline != nullptr;
    }
    
    Primitive* CreatePrimitiveFromGLTF(const GLTFPrimitive& gltf_primitive) {
        // 从GLTF primitive创建引擎Primitive
        // 每个GLTF primitive对应一个独立的引擎Primitive/Mesh
        
        // 获取位置数据
        auto pos_it = gltf_primitive.attributes.find("POSITION");
        if (pos_it == gltf_primitive.attributes.end()) {
            LOG_ERROR("GLTF primitive missing POSITION attribute");
            return nullptr;
        }
        
        const GLTFAccessor& pos_accessor = document.accessors[pos_it->second];
        const GLTFBufferView& pos_buffer_view = document.bufferViews[pos_accessor.bufferView];
        const GLTFBuffer& pos_buffer = document.buffers[pos_buffer_view.buffer];
        
        // 创建顶点数据
        uint32_t vertex_count = pos_accessor.count;
        std::vector<float> vertices(vertex_count * 3);
        
        const float* src_data = reinterpret_cast<const float*>(
            pos_buffer.data.data() + pos_buffer_view.byteOffset + pos_accessor.byteOffset
        );
        
        memcpy(vertices.data(), src_data, vertex_count * 3 * sizeof(float));
        
        // 使用WorkObject的CreatePrimitive方法
        VILConfig vil_config;
        vil_config.Add(VAN::Position, VF_V3F);
        
        // TODO: 未来可以添加更多顶点属性
        // - NORMAL: 法线数据
        // - TEXCOORD_0: UV坐标
        // - COLOR_0: 顶点颜色
        // - JOINTS_0, WEIGHTS_0: 骨骼动画数据
        
        std::vector<VertexAttribData> vad_list = {
            {VAN::Position, VF_V3F, vertices.data()}
        };
        
        return framework->CreatePrimitive("GLTF_Primitive", vertex_count, &vil_config, vad_list);
    }
    
    SceneNode* CreateSceneNodeFromGLTF(uint32_t node_index, Scene* scene) {
        // 将GLTF节点映射为引擎SceneNode
        // 保持GLTF的层次结构，每个GLTF节点对应一个SceneNode
        
        if (node_index >= document.nodes.size()) {
            return nullptr;
        }
        
        const GLTFNode& gltf_node = document.nodes[node_index];
        
        // 创建SceneNode
        SceneNode* scene_node = new SceneNode(scene);
        scene_node->SetNodeName(gltf_node.name.c_str());
        
        // 设置变换矩阵
        if (gltf_node.matrix.size() == 16) {
            Matrix4f transform;
            for (int i = 0; i < 16; i++) {
                transform.m[i / 4][i % 4] = gltf_node.matrix[i];
            }
            scene_node->SetLocalMatrix(transform);
        }
        // TODO: 支持TRS (Translation, Rotation, Scale) 分量
        
        // 如果节点有网格，创建MeshComponent
        // 按照需求：每个GLTF MESH封装成一个特殊的SCENE NODE
        if (gltf_node.mesh != UINT32_MAX && gltf_node.mesh < document.meshes.size()) {
            const GLTFMesh& gltf_mesh = document.meshes[gltf_node.mesh];
            
            // 为每个primitive创建独立的子节点
            // 按照需求：每个独立GLTF primitive都创建独立的PRIMITIVE/MESH
            for (size_t i = 0; i < gltf_mesh.primitives.size(); i++) {
                const GLTFPrimitive& gltf_primitive = gltf_mesh.primitives[i];
                
                // 创建Primitive
                Primitive* primitive = CreatePrimitiveFromGLTF(gltf_primitive);
                if (!primitive) {
                    continue;
                }
                
                // 创建Mesh
                Mesh* mesh = framework->CreateMesh(primitive, material_instance, pipeline);
                if (!mesh) {
                    continue;
                }
                
                // 创建primitive的子节点
                SceneNode* primitive_node = new SceneNode(scene);
                std::string primitive_name = gltf_mesh.name + "_Primitive_" + std::to_string(i);
                primitive_node->SetNodeName(primitive_name.c_str());
                
                // 创建MeshComponent
                CreateComponentInfo cci(primitive_node);
                framework->CreateComponent<MeshComponent>(&cci, mesh);
                
                // 添加为子节点
                scene_node->Add(primitive_node);
                created_nodes.push_back(primitive_node);
            }
        }
        
        // 递归处理子节点，保持GLTF层次结构
        for (uint32_t child_index : gltf_node.children) {
            SceneNode* child_node = CreateSceneNodeFromGLTF(child_index, scene);
            if (child_node) {
                scene_node->Add(child_node);
                created_nodes.push_back(child_node);
            }
        }
        
        return scene_node;
    }
    
    bool BuildSceneHierarchy(Scene* scene, SceneNode* root_node) {
        if (document.scenes.empty()) {
            LOG_ERROR("No scenes found in GLTF document");
            return false;
        }
        
        const GLTFScene& gltf_scene = document.scenes[document.scene];
        
        // 为场景中的每个根节点创建SceneNode
        for (uint32_t node_index : gltf_scene.nodes) {
            SceneNode* scene_node = CreateSceneNodeFromGLTF(node_index, scene);
            if (scene_node) {
                root_node->Add(scene_node);
                created_nodes.push_back(scene_node);
            }
        }
        
        return true;
    }
};

// 测试应用程序
class GLTFLoaderTestApp : public WorkObject {
private:
    GLTFLoader* gltf_loader = nullptr;
    
public:
    using WorkObject::WorkObject;
    
    ~GLTFLoaderTestApp() {
        SAFE_CLEAR(gltf_loader);
    }
    
    bool Init() override {
        // 创建GLTF加载器
        gltf_loader = new GLTFLoader(this);
        
        // 初始化渲染资源
        if (!gltf_loader->InitializeRenderResources()) {
            LOG_ERROR("Failed to initialize GLTF render resources");
            return false;
        }
        
        // 加载测试数据
        if (!gltf_loader->LoadTestData()) {
            LOG_ERROR("Failed to load GLTF test data");
            return false;
        }
        
        // 构建场景层次结构
        if (!gltf_loader->BuildSceneHierarchy(GetScene(), GetSceneRoot())) {
            LOG_ERROR("Failed to build scene hierarchy");
            return false;
        }
        
        // 设置摄像机
        CameraControl* camera_control = GetCameraControl();
        camera_control->SetPosition(Vector3f(2, 2, 2));
        camera_control->SetTarget(Vector3f(0, 0, 0));
        
        LOG_INFO("GLTF Loader Test initialized successfully");
        return true;
    }
};

int os_main(int, os_char**) {
    return RunFramework<GLTFLoaderTestApp>(OS_TEXT("GLTF Model Loader Test"), 1280, 720);
}