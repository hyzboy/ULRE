/**
 * 测试 LuminousFlux 类的功能
 */

#include <hgl/math/Luminance.h>
#include <iostream>

using namespace hgl::math;

int main()
{
    // 测试 LuminousFlux 创建
    LuminousFlux bulb60w = LuminousFlux::FromLumen(800.0f);
    std::cout << "60W 灯泡光通量: " << bulb60w.AsLumen() << " 流明" << std::endl;
    
    // 测试转换为坎德拉
    float candela = bulb60w.ToCandela();
    std::cout << "对应光强度: " << candela << " 坎德拉" << std::endl;
    
    // 测试计算照度
    float illuminance_1m = bulb60w.CalculateIlluminance(1.0f);
    float illuminance_2m = bulb60w.CalculateIlluminance(2.0f);
    std::cout << "1米处照度: " << illuminance_1m << " 勒克斯" << std::endl;
    std::cout << "2米处照度: " << illuminance_2m << " 勒克斯" << std::endl;
    
    // 测试操作符
    LuminousFlux bulb40w = LuminousFlux::FromLumen(450.0f);
    LuminousFlux combined = bulb60w + bulb40w;
    std::cout << "两个灯泡组合: " << combined.AsLumen() << " 流明" << std::endl;
    
    // 测试光源数据
    std::cout << "\n光源数据测试:" << std::endl;
    std::cout << LightSources::Incandescent60W.name << ": " 
              << LightSources::Incandescent60W.luminousFlux.AsLumen() << " 流明" << std::endl;
    std::cout << LightSources::LED9W_WarmWhite.name << ": " 
              << LightSources::LED9W_WarmWhite.luminousFlux.AsLumen() << " 流明" << std::endl;
    
    return 0;
}
