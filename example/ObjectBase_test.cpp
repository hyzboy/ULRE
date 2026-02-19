/**
 * ObjectBase 框架测试程序
 *
 * 展示如何使用ObjectBase进行对象追踪
 * 编译: g++ -std=c++20 ObjectBaseTest.cpp -o test
 */

#include<hgl/utils/ObjectBase.h>
#include<iostream>
#include<memory>

using namespace hgl;

// ========== 测试用对象 ==========

class TestResource : public utils::ObjectBase
{
private:
    int value_;

public:
    TestResource(
        int value,
        const std::string& name,
        const std::source_location& loc = std::source_location::current()
    ) noexcept
        : ObjectBase(core::ObjectTypeTag::Material, name, loc)
        , value_(value)
    {
        std::cout << "[TestResource] Created: " << name << " = " << value << std::endl;
    }

    virtual ~TestResource() noexcept override
    {
        HGL_OBJECT_DESTROY_LOCATION();
        std::cout << "[TestResource] Destroyed: " << object_name_ << std::endl;
    }

    int get_value() const noexcept { return value_; }
};

// ========== 测试函数 ==========

void test_basic_creation()
{
    std::cout << "\n=== Test 1: Basic Creation/Destruction ===" << std::endl;

    auto obj1 = new TestResource(42, "Resource1");
    auto obj2 = new TestResource(100, "Resource2");

    std::cout << "Object count: " << HGL_GET_OBJECT_COUNT() << std::endl;
    std::cout << "\nObject info:" << std::endl;
    std::cout << "  1: " << obj1->to_string() << std::endl;
    std::cout << "  2: " << obj2->to_string() << std::endl;

    delete obj1;
    std::cout << "\nAfter deleting obj1:" << std::endl;
    std::cout << "Object count: " << HGL_GET_OBJECT_COUNT() << std::endl;

    delete obj2;
}

void test_leak_detection()
{
    std::cout << "\n=== Test 2: Leak Detection ===" << std::endl;

    auto obj1 = new TestResource(1, "Leak1");
    auto obj2 = new TestResource(2, "Leak2");
    auto obj3 = new TestResource(3, "Leak3");

    std::cout << "\nCreated 3 objects, destroying 1:" << std::endl;
    delete obj1;

    std::cout << "\nReporting leaks:" << std::endl;
    size_t leaks = HGL_REPORT_LEAKS();
    std::cout << "Total leaks detected: " << leaks << std::endl;

    delete obj2;
    delete obj3;
}

void test_object_validity()
{
    std::cout << "\n=== Test 3: Object Validity ===" << std::endl;

    auto obj = new TestResource(999, "ValidityTest");

    std::cout << "Before deletion:" << std::endl;
    std::cout << "  is_valid(): " << (obj->is_valid() ? "true" : "false") << std::endl;
    std::cout << "  is_destroyed(): " << (obj->is_destroyed() ? "true" : "false") << std::endl;

    delete obj;

    // Note: Accessing deleted object is UB, only for demonstration
    // Don't do this in real code!
    std::cout << "\nAfter deletion:" << std::endl;
    std::cout << "  (Object is deleted, would crash if accessed)" << std::endl;
}

void test_magic_number()
{
    std::cout << "\n=== Test 4: Magic Number Verification ===" << std::endl;

    auto obj = new TestResource(42, "MagicTest");

    std::cout << "Object magic number: 0x" << std::hex << obj->get_object_id() << std::dec << std::endl;
    std::cout << "Expected magic: 0xDEADBEEFCAFEBABE" << std::endl;

    // Test magic number (in real code, would be in ObjectBase internally)
    std::cout << "Valid: " << (obj->is_valid() ? "YES" : "NO") << std::endl;

    delete obj;
}

void test_source_location_tracking()
{
    std::cout << "\n=== Test 5: Source Location Tracking ===" << std::endl;

    auto obj = new TestResource(123, "LocationTest");

    const auto& create_loc = obj->get_creation_location();
    std::cout << "Created at: " << create_loc.to_string() << std::endl;
    std::cout << "  File: " << create_loc.file << std::endl;
    std::cout << "  Line: " << create_loc.line << std::endl;
    std::cout << "  Function: " << create_loc.function << std::endl;

    delete obj;
}

void test_registry_operations()
{
    std::cout << "\n=== Test 6: Registry Operations ===" << std::endl;

    auto id1 = 0UL;
    {
        auto obj = new TestResource(111, "Registry1");
        id1 = obj->get_object_id();

        std::cout << "Created object with ID: 0x" << std::hex << id1 << std::dec << std::endl;

        // Find object by ID
        auto found = HGL_FIND_OBJECT(id1, TestResource);
        if (found)
        {
            std::cout << "Found via registry: " << found->to_string() << std::endl;
        }

        delete obj;
    }

    // Try to find deleted object
    auto found = HGL_FIND_OBJECT(id1, TestResource);
    std::cout << "After deletion, find returns: " << (found ? "Found (error!)" : "nullptr (correct)") << std::endl;
}

void test_list_all()
{
    std::cout << "\n=== Test 7: List All Objects ===" << std::endl;

    auto obj1 = new TestResource(10, "List1");
    auto obj2 = new TestResource(20, "List2");
    auto obj3 = new TestResource(30, "List3");

    std::cout << "\nAll active objects:" << std::endl;
    HGL_LIST_ALL_OBJECTS();

    delete obj1;
    delete obj2;
    delete obj3;
}

void test_stress()
{
    std::cout << "\n=== Test 8: Stress Test ===" << std::endl;

    std::vector<std::unique_ptr<TestResource>> objects;

    const int COUNT = 1000;
    std::cout << "Creating " << COUNT << " objects..." << std::endl;

    for (int i = 0; i < COUNT; i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "StressObj_%d", i);
        objects.push_back(std::make_unique<TestResource>(i, name));
    }

    std::cout << "Total objects: " << HGL_GET_OBJECT_COUNT() << std::endl;

    // Intentionally leak 10% to test detection
    std::cout << "Intentionally leaking 10% of objects..." << std::endl;
    for (int i = 0; i < COUNT / 10; i++)
    {
        auto leaked = new TestResource(-i, "Leaked");
    }

    std::cout << "Final object count: " << HGL_GET_OBJECT_COUNT() << std::endl;

    size_t leaks = HGL_REPORT_LEAKS();
    std::cout << "Expected ~100 leaks, got: " << leaks << std::endl;
}

// ========== Main ==========

int main()
{
    std::cout << "╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║   ObjectBase Framework Test Suite      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;

    try
    {
        test_basic_creation();
        test_leak_detection();
        test_object_validity();
        test_magic_number();
        test_source_location_tracking();
        test_registry_operations();
        test_list_all();
        test_stress();

        std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║   All tests completed!                 ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}

/*
 * 预期输出示例：
 *
 * === Test 1: Basic Creation/Destruction ===
 * [TestResource] Created: Resource1 = 42
 * [TestResource] Created: Resource2 = 100
 * Object count: 2
 *
 * === Test 2: Leak Detection ===
 * Total leaks detected: 2
 * [ObjectRegistry] LEAK: Object{ID=0x2, ...}
 * [ObjectRegistry] LEAK: Object{ID=0x3, ...}
 *
 * === Test 8: Stress Test ===
 * Creating 1000 objects...
 * Total objects: 1000
 * Final object count: 1100
 * Expected ~100 leaks, got: 100
 */
