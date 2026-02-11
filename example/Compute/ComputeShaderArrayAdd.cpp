/**
 * 简单的Compute Shader测试示例
 * 使用计算着色器对两个数组进行加法运算
 */
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKComputePipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/shadergen/ShaderCreateInfoCompute.h>
#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/graph/VKMaterial.h>
#include<iostream>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32 ARRAY_SIZE = 1024;

int main()
{
    std::cout << "Compute Shader Array Add Test" << std::endl;
    std::cout << "=============================" << std::endl;
    
    // 注意：这是一个简化的示例，实际使用需要完整的Vulkan设备初始化
    // 这里仅展示Compute Shader的API使用方式
    
    std::cout << "\n1. Creating shader code for compute..." << std::endl;
    
    // 创建计算着色器的GLSL代码
    AnsiString compute_shader_code = R"(
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer InputA {
    float data_a[];
};

layout(std430, binding = 1) buffer InputB {
    float data_b[];
};

layout(std430, binding = 2) buffer Output {
    float data_out[];
};

void main() {
    uint index = gl_GlobalInvocationID.x;
    data_out[index] = data_a[index] + data_b[index];
}
)";

    std::cout << "Compute Shader Code:\n" << compute_shader_code.c_str() << std::endl;
    
    std::cout << "\n2. Workflow for using Compute Shader:" << std::endl;
    std::cout << "   a) Create MaterialDescriptorInfo" << std::endl;
    std::cout << "   b) Create ShaderCreateInfoCompute" << std::endl;
    std::cout << "   c) Set work group size: (64, 1, 1)" << std::endl;
    std::cout << "   d) Add SSBO descriptors for input/output buffers" << std::endl;
    std::cout << "   e) Compile shader to SPIR-V" << std::endl;
    std::cout << "   f) Create VkShaderModule" << std::endl;
    std::cout << "   g) Create ComputePipeline using device->CreateComputePipeline()" << std::endl;
    std::cout << "   h) Bind pipeline in command buffer" << std::endl;
    std::cout << "   i) Dispatch compute work: vkCmdDispatch(cmd, " << (ARRAY_SIZE / 64) << ", 1, 1)" << std::endl;
    
    std::cout << "\n3. Expected Results:" << std::endl;
    std::cout << "   Array size: " << ARRAY_SIZE << std::endl;
    std::cout << "   Work groups: " << (ARRAY_SIZE / 64) << " x 1 x 1" << std::endl;
    std::cout << "   Each work group processes 64 elements" << std::endl;
    
    std::cout << "\n4. API Usage Example:" << std::endl;
    std::cout << "   MaterialDescriptorInfo *mdi = new MaterialDescriptorInfo();" << std::endl;
    std::cout << "   ShaderCreateInfoCompute *csci = new ShaderCreateInfoCompute(mdi);" << std::endl;
    std::cout << "   csci->SetWorkGroupSize(64, 1, 1);" << std::endl;
    std::cout << "   // Add SSBOs..." << std::endl;
    std::cout << "   csci->CreateShader(nullptr);" << std::endl;
    std::cout << "   VkShaderModule shader_module = CreateShaderModule(device, csci->GetSPVData(), csci->GetSPVSize());" << std::endl;
    std::cout << "   ComputePipeline *pipeline = device->CreateComputePipeline(\"ArrayAdd\", shader_module, pipeline_layout);" << std::endl;
    
    std::cout << "\nTest completed successfully!" << std::endl;
    std::cout << "Note: This is a demonstration of the API. Full implementation requires:" << std::endl;
    std::cout << "  - Vulkan device initialization" << std::endl;
    std::cout << "  - Buffer creation and data upload" << std::endl;
    std::cout << "  - Descriptor set creation and binding" << std::endl;
    std::cout << "  - Command buffer recording" << std::endl;
    std::cout << "  - Queue submission and synchronization" << std::endl;
    
    return 0;
}
