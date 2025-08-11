#include <hgl/example/ExclusiveEventDispatcherExample.h>
#include <iostream>

/**
 * 测试独占模式事件分发器功能
 */
int main()
{
    std::cout << "=== EventDispatcher 独占模式功能演示 ===" << std::endl;
    std::cout << "This demonstrates how EventDispatcher exclusive mode works." << std::endl;
    std::cout << "独占模式允许某个事件分发器在更高级别暂时接管所有事件处理。" << std::endl << std::endl;
    
    try
    {
        hgl::example::ExclusiveEventDispatcherExample::DemonstrateExclusiveMode();
        
        std::cout << std::endl << "=== 演示完成 ===" << std::endl;
        std::cout << "独占模式功能演示成功完成！" << std::endl;
        std::cout << "Exclusive mode demonstration completed successfully!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}