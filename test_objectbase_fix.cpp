// Test compilation of ObjectBase.h with ObjectTracker.h integration
#include <iostream>
#include <hgl/core/ObjectType.h>
#include <hgl/utils/ObjectBase.h>

using namespace hgl::utils;
using namespace hgl::core;

// Test Fence class inheriting from ObjectBase
class TestFence : public ObjectBase {
public:
    TestFence(const std::source_location& loc = std::source_location::current())
        : ObjectBase(ObjectTypeTag::VKFence, "TestFence", loc)
    {
        std::cout << "TestFence created with ID: 0x" << std::hex << get_object_id() << std::endl;
    }
    
    virtual ~TestFence() override {
        HGL_OBJECT_DESTROY_LOCATION();
        std::cout << "TestFence destroyed" << std::endl;
    }
};

int main() {
    std::cout << "=== ObjectBase + ObjectTracker Integration Test ===" << std::endl;
    
    // Create some test objects
    TestFence* fence1 = new TestFence();
    TestFence* fence2 = new TestFence();
    
    std::cout << "\n=== Active Objects ===" << std::endl;
    HGL_LIST_ALL_OBJECTS();
    
    std::cout << "\nTotal objects: " << HGL_GET_OBJECT_COUNT() << std::endl;
    
    // Clean up
    delete fence1;
    delete fence2;
    
    std::cout << "\n=== After Cleanup ===" << std::endl;
    HGL_LIST_ALL_OBJECTS();
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}
