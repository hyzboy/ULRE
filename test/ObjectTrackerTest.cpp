#include <hgl/utils/ObjectTracker.h>
#include <hgl/vk/VKObjectNameBuilder.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hgl::utils;
using namespace hgl::graph;

/**
 * 该测试演示如何使用 ObjectTracker 系统追踪对象分配
 */

// 模拟的Vulkan设备
class MockVulkanDevice {
public:
    void* CreateBuffer(uint64_t id, size_t size) {
        HGL_CAPTURE_SCOPE();
        
        std::cout << "[Device] Creating buffer: id=" << id 
                  << ", size=" << size << std::endl;
        
        // 模拟一些工作
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        
        return (void*)(uintptr_t)id;
    }
};

// 模拟的渲染系统
class MockRenderSystem {
private:
    MockVulkanDevice& device;
    
public:
    MockRenderSystem(MockVulkanDevice& dev) : device(dev) {}
    
    uint64_t AllocateGeometryBuffer() {
        HGL_CAPTURE_SCOPE();
        
        uint64_t id = HGL_TRACK_ALLOCATION(
            "Geometry Buffer",
            ObjectTypeTag::VertexBuffer
        );
        
        std::cout << "[RenderSystem] Allocating geometry buffer: id=" << id << std::endl;
        device.CreateBuffer(id, 1024 * 1024);
        
        return id;
    }
    
    uint64_t AllocateIndirectBuffer() {
        HGL_CAPTURE_SCOPE();
        
        uint64_t id = HGL_TRACK_ALLOCATION(
            "IndirectDraw Commands",
            ObjectTypeTag::IndirectDrawBuffer
        );
        
        std::cout << "[RenderSystem] Allocating indirect buffer: id=" << id << std::endl;
        device.CreateBuffer(id, 64 * 1024);
        
        return id;
    }
};

// 用户代码
class SceneRenderer {
private:
    MockRenderSystem& render_system;
    
public:
    SceneRenderer(MockRenderSystem& sys) : render_system(sys) {}
    
    void Setup() {
        HGL_CAPTURE_SCOPE();
        
        std::cout << "[SceneRenderer] Setting up scene..." << std::endl;
        
        for (int i = 0; i < 3; i++) {
            auto buf_id = render_system.AllocateGeometryBuffer();
            std::cout << "  Created buffer: " << buf_id << std::endl;
        }
        
        auto indirect_id = render_system.AllocateIndirectBuffer();
        std::cout << "  Created indirect buffer: " << indirect_id << std::endl;
    }
};

// 测试函数
void test_basic_allocation() {
    std::cout << "\n=== Test 1: Basic Allocation ===" << std::endl;
    
    MockVulkanDevice device;
    MockRenderSystem render_sys(device);
    SceneRenderer scene(render_sys);
    
    scene.Setup();
    
    std::cout << "Total tracked events: " 
              << g_object_tracker->get_event_count() << std::endl;
}

void test_multithreaded_allocation() {
    std::cout << "\n=== Test 2: Multi-threaded Allocation ===" << std::endl;
    
    MockVulkanDevice device;
    MockRenderSystem render_sys(device);
    
    auto worker = [&render_sys](int thread_id) {
        HGL_CAPTURE_SCOPE();  // 捕获线程入口
        
        std::cout << "[Thread " << thread_id << "] Starting..." << std::endl;
        
        for (int i = 0; i < 3; i++) {
            auto id = render_sys.AllocateGeometryBuffer();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        std::cout << "[Thread " << thread_id << "] Done" << std::endl;
    };
    
    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    
    t1.join();
    t2.join();
    
    std::cout << "Total tracked events: " 
              << g_object_tracker->get_event_count() << std::endl;
}

void test_dump() {
    std::cout << "\n=== Test 3: Dump and Reload ===" << std::endl;
    
    MockVulkanDevice device;
    MockRenderSystem render_sys(device);
    SceneRenderer scene(render_sys);
    
    scene.Setup();
    
    const char* dump_file = "test_trace.bin";
    std::cout << "Dumping to: " << dump_file << std::endl;
    g_object_tracker->dump_to_file(dump_file);
    
    std::cout << "Dump complete. Use analyze_trace.py to query:" << std::endl;
    std::cout << "  python3 analyze_trace.py " << dump_file << " stats" << std::endl;
    std::cout << "  python3 analyze_trace.py " << dump_file << " list 20" << std::endl;
}

int main() {
    std::cout << "=== ObjectTracker Test Suite ===" << std::endl;
    
    // 初始化追踪系统
    initialize_object_tracker();
    std::cout << "ObjectTracker initialized" << std::endl;
    
    try {
        test_basic_allocation();
        test_multithreaded_allocation();
        test_dump();
        
        std::cout << "\n=== All tests completed ===" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    // 清理
    shutdown_object_tracker();
    
    return 0;
}
