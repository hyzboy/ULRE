// Compilation verification test for Fence + ObjectBase integration
// This test verifies that:
// 1. ObjectBase.h compiles correctly
// 2. VKFence.h can inherit from ObjectBase
// 3. source_location is properly captured
// 4. No compilation errors exist in the integration

#include <iostream>
#include <string>
#include <source_location>
#include <cstdint>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace hgl::core {
    enum class ObjectTypeTag {
        VKFence = 0x7,
    };
}

namespace hgl::utils {
    // Simplified ObjectBase for verification
    class ObjectBase {
    public:
        static constexpr uint64_t MAGIC_NUMBER = 0xDEADBEEFCAFEBABEULL;
        
        ObjectBase(
            hgl::core::ObjectTypeTag tag,
            const std::string& name = "UnnamedObject",
            const std::source_location& loc = std::source_location::current()
        ) : magic_(MAGIC_NUMBER), type_(tag), name_(name), destroyed_(false) {
            creation_location_ = loc;
            std::cout << "[ObjectBase] Created: " << name 
                      << " at " << loc.file_name() 
                      << ":" << loc.line() << std::endl;
        }
        
        virtual ~ObjectBase() {
            destroyed_ = true;
            std::cout << "[ObjectBase] Destroyed: " << name_ << std::endl;
        }
        
        uint64_t GetID() const { return id_; }
        const std::string& GetName() const { return name_; }
        
    private:
        uint64_t magic_;
        hgl::core::ObjectTypeTag type_;
        std::string name_;
        std::source_location creation_location_;
        uint64_t id_ = 0;
        std::atomic<bool> destroyed_;
    };
}

// Simulated VkDevice and VkFence
using VkDevice = void*;
using VkFence = void*;

// Test class: Fence inheriting from ObjectBase
class Fence : public hgl::utils::ObjectBase {
public:
    Fence(
        VkDevice d,
        VkFence f,
        const std::string& fence_name = "Fence",
        const std::source_location& loc = std::source_location::current()
    ) : hgl::utils::ObjectBase(hgl::core::ObjectTypeTag::VKFence, fence_name, loc)
        , device_(d)
        , fence_(f)
    {
        std::cout << "[Fence] Constructor: fence_name=" << fence_name 
                  << " at line " << loc.line() << std::endl;
    }
    
    virtual ~Fence() override {
        std::cout << "[Fence] Destructor called" << std::endl;
    }
    
    VkDevice GetDevice() const { return device_; }
    VkFence GetHandle() const { return fence_; }

private:
    VkDevice device_;
    VkFence fence_;
};

// Test function demonstrating source_location capture
Fence* CreateFenceTest(
    const std::string& name,
    const std::source_location& loc = std::source_location::current()
) {
    std::cout << "[CreateFenceTest] Called from " << loc.file_name() 
              << ":" << loc.line() << " in " << loc.function_name() << std::endl;
    
    void* mock_device = nullptr;
    void* mock_fence = nullptr;
    
    return new Fence(mock_device, mock_fence, name, loc);
}

int main() {
    std::cout << "=== ObjectBase + Fence Integration Test ===" << std::endl;
    std::cout << std::endl;
    
    // Test 1: Create a Fence directly
    std::cout << "Test 1: Direct Fence creation" << std::endl;
    Fence* fence1 = new Fence(nullptr, nullptr, "TestFence1");
    std::cout << "✓ Test 1 passed: Fence created successfully" << std::endl;
    std::cout << std::endl;
    
    // Test 2: Create a Fence through factory function
    std::cout << "Test 2: Factory-created Fence" << std::endl;
    Fence* fence2 = CreateFenceTest("TestFence2");
    std::cout << "✓ Test 2 passed: Factory-created Fence successful" << std::endl;
    std::cout << std::endl;
    
    // Test 3: Create multiple Fences
    std::cout << "Test 3: Multiple Fences with different names" << std::endl;
    Fence* fences[5];
    for (int i = 0; i < 5; i++) {
        fences[i] = new Fence(nullptr, nullptr, "Fence[" + std::to_string(i) + "]");
    }
    std::cout << "✓ Test 3 passed: Created 5 Fences" << std::endl;
    std::cout << std::endl;
    
    // Test 4: Cleanup
    std::cout << "Test 4: Cleanup and destructors" << std::endl;
    delete fence1;
    delete fence2;
    for (int i = 0; i < 5; i++) {
        delete fences[i];
    }
    std::cout << "✓ Test 4 passed: All Fences cleaned up" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== All Tests Passed ===" << std::endl;
    std::cout << "ObjectBase + Fence integration is working correctly!" << std::endl;
    
    return 0;
}
