#include<hgl/component/OfflineMeshComponent.h>
#include<hgl/graph/DirectMeshRenderer.h>
#include<hgl/graph/OfflineMeshExample.h>
#include<iostream>

/**
 * Simple unit test for OfflineMeshComponent and DirectMeshRenderer
 * 
 * This test verifies basic functionality without requiring a full Vulkan setup
 */

namespace hgl { namespace graph {

class MockVulkanDevice : public VulkanDevice
{
public:
    // Minimal mock implementation for testing
    MockVulkanDevice() : VulkanDevice(nullptr) {}
};

class MockMaterial : public Material
{
public:
    MockMaterial() : Material(nullptr) {}
};

class MockPipeline : public Pipeline
{
public:
    MockPipeline() : Pipeline(nullptr, nullptr) {}
};

class MockMesh : public Mesh
{
private:
    MeshDataBuffer data_buffer;
    MeshRenderData render_data;
    
public:
    MockMesh() : Mesh(nullptr, nullptr, nullptr)
    {
        // Initialize mock data
        render_data.vertex_count = 100;
        render_data.first_vertex = 0;
        render_data.index_count = 150;
        render_data.first_index = 0;
        render_data.vertex_offset = 0;
    }
    
    const MeshDataBuffer* GetDataBuffer() const override { return &data_buffer; }
    const MeshRenderData* GetRenderData() const override { return &render_data; }
};

} }

bool TestOfflineMeshComponentBasic()
{
    using namespace hgl::graph;
    
    std::cout << "Testing OfflineMeshComponent basic functionality..." << std::endl;
    
    // Create mock mesh
    MockMesh* mock_mesh = new MockMesh();
    
    // Create offline component data
    OfflineMeshComponentData* data = new OfflineMeshComponentData(mock_mesh);
    
    // Test instance data setting
    data->SetInstanceData(10, 5);
    
    // Verify data
    if (data->instance_count != 10 || data->first_instance != 5)
    {
        std::cout << "FAILED: Instance data not set correctly" << std::endl;
        delete data;
        delete mock_mesh;
        return false;
    }
    
    // Create component
    ComponentDataPtr cdp(data);
    OfflineMeshComponentManager manager;
    OfflineMeshComponent* component = new OfflineMeshComponent(cdp, &manager);
    
    // Test data access
    if (component->GetInstanceCount() != 10 || component->GetFirstInstance() != 5)
    {
        std::cout << "FAILED: Component data access incorrect" << std::endl;
        delete component;
        delete mock_mesh;
        return false;
    }
    
    // Test mesh access
    if (component->GetMesh() != mock_mesh)
    {
        std::cout << "FAILED: Mesh access incorrect" << std::endl;
        delete component;
        delete mock_mesh;
        return false;
    }
    
    // Cleanup
    delete component;
    delete mock_mesh;
    
    std::cout << "PASSED: OfflineMeshComponent basic functionality" << std::endl;
    return true;
}

bool TestOfflineMeshComponentDrawCommands()
{
    using namespace hgl::graph;
    
    std::cout << "Testing OfflineMeshComponent draw commands..." << std::endl;
    
    MockMesh* mock_mesh = new MockMesh();
    OfflineMeshComponentData* data = new OfflineMeshComponentData(mock_mesh);
    
    // Test draw commands
    VkDrawIndirectCommand draw_cmds[2];
    draw_cmds[0].vertexCount = 100;
    draw_cmds[0].instanceCount = 1;
    draw_cmds[0].firstVertex = 0;
    draw_cmds[0].firstInstance = 0;
    
    draw_cmds[1].vertexCount = 200;
    draw_cmds[1].instanceCount = 2;
    draw_cmds[1].firstVertex = 100;
    draw_cmds[1].firstInstance = 1;
    
    data->SetDrawCommands(draw_cmds, 2);
    
    // Create component and test access
    ComponentDataPtr cdp(data);
    OfflineMeshComponentManager manager;
    OfflineMeshComponent* component = new OfflineMeshComponent(cdp, &manager);
    
    uint32_t command_count = 0;
    const VkDrawIndirectCommand* retrieved_cmds = component->GetDrawCommands(command_count);
    
    if (command_count != 2 || !retrieved_cmds)
    {
        std::cout << "FAILED: Draw commands not retrieved correctly" << std::endl;
        delete component;
        delete mock_mesh;
        return false;
    }
    
    if (retrieved_cmds[0].vertexCount != 100 || retrieved_cmds[1].instanceCount != 2)
    {
        std::cout << "FAILED: Draw command data incorrect" << std::endl;
        delete component;
        delete mock_mesh;
        return false;
    }
    
    // Test has offline data
    if (!component->HasOfflineData())
    {
        std::cout << "FAILED: HasOfflineData should return true" << std::endl;
        delete component;
        delete mock_mesh;
        return false;
    }
    
    delete component;
    delete mock_mesh;
    
    std::cout << "PASSED: OfflineMeshComponent draw commands" << std::endl;
    return true;
}

bool TestDirectMeshRendererBasic()
{
    using namespace hgl::graph;
    
    std::cout << "Testing DirectMeshRenderer basic functionality..." << std::endl;
    
    MockVulkanDevice* mock_device = new MockVulkanDevice();
    DirectMeshRenderer* renderer = new DirectMeshRenderer(mock_device);
    
    // Test basic state
    if (!renderer)
    {
        std::cout << "FAILED: Could not create DirectMeshRenderer" << std::endl;
        delete mock_device;
        return false;
    }
    
    // Test clear
    renderer->Clear();
    
    delete renderer;
    delete mock_device;
    
    std::cout << "PASSED: DirectMeshRenderer basic functionality" << std::endl;
    return true;
}

bool TestOfflineBatchBuilder()
{
    using namespace hgl::graph;
    
    std::cout << "Testing OfflineBatchBuilder..." << std::endl;
    
    MockVulkanDevice* mock_device = new MockVulkanDevice();
    MockMaterial* mock_material = new MockMaterial();
    MockPipeline* mock_pipeline = new MockPipeline();
    
    OfflineBatchBuilder builder(mock_device, mock_material, mock_pipeline);
    
    // Test basic builder functionality
    builder.Clear();
    
    delete mock_pipeline;
    delete mock_material;
    delete mock_device;
    
    std::cout << "PASSED: OfflineBatchBuilder basic functionality" << std::endl;
    return true;
}

int main()
{
    std::cout << "Running OfflineMeshComponent Unit Tests..." << std::endl;
    std::cout << "==========================================" << std::endl;
    
    bool all_passed = true;
    
    all_passed &= TestOfflineMeshComponentBasic();
    all_passed &= TestOfflineMeshComponentDrawCommands();
    all_passed &= TestDirectMeshRendererBasic();
    all_passed &= TestOfflineBatchBuilder();
    
    std::cout << "==========================================" << std::endl;
    if (all_passed)
    {
        std::cout << "ALL TESTS PASSED!" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "SOME TESTS FAILED!" << std::endl;
        return 1;
    }
}