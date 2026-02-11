/**
 * Image Processing with Compute Shader Example
 * Demonstrates a 2D image blur filter using compute shaders
 */
#include<iostream>

using namespace std;

int main()
{
    cout << "=== Compute Shader Image Blur Example ===" << endl;
    cout << "\nThis example demonstrates a Gaussian blur filter using compute shaders.\n" << endl;
    
    // Image dimensions
    const int IMAGE_WIDTH = 1024;
    const int IMAGE_HEIGHT = 1024;
    const int WORK_GROUP_SIZE = 16;
    
    cout << "Image Configuration:" << endl;
    cout << "  Resolution: " << IMAGE_WIDTH << "x" << IMAGE_HEIGHT << endl;
    cout << "  Work group size: " << WORK_GROUP_SIZE << "x" << WORK_GROUP_SIZE << endl;
    cout << "  Number of work groups: " 
         << (IMAGE_WIDTH / WORK_GROUP_SIZE) << "x" 
         << (IMAGE_HEIGHT / WORK_GROUP_SIZE) << endl;
    
    cout << "\n=== GLSL Compute Shader Code ===" << endl;
    cout << R"(
#version 450

// Work group configuration for 2D image processing
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Input image
layout(binding = 0, rgba8) uniform readonly image2D inputImage;

// Output image
layout(binding = 1, rgba8) uniform writeonly image2D outputImage;

// Gaussian blur kernel (3x3)
const float kernel[9] = float[](
    1.0/16.0, 2.0/16.0, 1.0/16.0,
    2.0/16.0, 4.0/16.0, 2.0/16.0,
    1.0/16.0, 2.0/16.0, 1.0/16.0
);

void main() {
    // Get the current pixel coordinates
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
    
    // Get image dimensions
    ivec2 imageSize = imageSize(inputImage);
    
    // Check if we're within bounds
    if (coords.x >= imageSize.x || coords.y >= imageSize.y) {
        return;
    }
    
    // Apply 3x3 Gaussian blur
    vec4 result = vec4(0.0);
    int kernelIndex = 0;
    
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            ivec2 sampleCoords = coords + ivec2(x, y);
            
            // Handle edge cases by clamping
            sampleCoords = clamp(sampleCoords, ivec2(0), imageSize - 1);
            
            // Sample and accumulate
            vec4 sample = imageLoad(inputImage, sampleCoords);
            result += sample * kernel[kernelIndex];
            kernelIndex++;
        }
    }
    
    // Write result to output image
    imageStore(outputImage, coords, result);
}
)" << endl;
    
    cout << "\n=== API Usage Pattern ===" << endl;
    cout << "\n1. Setup Phase:" << endl;
    cout << "   // Create descriptor info" << endl;
    cout << "   MaterialDescriptorInfo *mdi = new MaterialDescriptorInfo();" << endl;
    cout << "   ShaderCreateInfoCompute *csci = new ShaderCreateInfoCompute(mdi);" << endl;
    
    cout << "\n2. Configure Shader:" << endl;
    cout << "   // Set work group size for 2D image" << endl;
    cout << "   csci->SetWorkGroupSize(16, 16, 1);" << endl;
    
    cout << "\n3. Add Image Descriptors:" << endl;
    cout << "   // Input image descriptor" << endl;
    cout << "   TextureDescriptor inputDesc;" << endl;
    cout << "   inputDesc.name = \"inputImage\";" << endl;
    cout << "   inputDesc.binding = 0;" << endl;
    cout << "   csci->AddTexture(DescriptorSetType::Value, &inputDesc);" << endl;
    cout << "" << endl;
    cout << "   // Output image descriptor" << endl;
    cout << "   TextureDescriptor outputDesc;" << endl;
    cout << "   outputDesc.name = \"outputImage\";" << endl;
    cout << "   outputDesc.binding = 1;" << endl;
    cout << "   csci->AddTexture(DescriptorSetType::Value, &outputDesc);" << endl;
    
    cout << "\n4. Compile and Create Pipeline:" << endl;
    cout << "   csci->CreateShader(nullptr);" << endl;
    cout << "   VkShaderModule module = CreateModule(csci->GetSPVData(), csci->GetSPVSize());" << endl;
    cout << "   ComputePipeline *pipeline = device->CreateComputePipeline(\"ImageBlur\", module, layout);" << endl;
    
    cout << "\n5. Execute Compute:" << endl;
    cout << "   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);" << endl;
    cout << "   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, ...);" << endl;
    cout << "   vkCmdDispatch(cmd, " 
         << (IMAGE_WIDTH / WORK_GROUP_SIZE) << ", "
         << (IMAGE_HEIGHT / WORK_GROUP_SIZE) << ", 1);" << endl;
    
    cout << "\n=== Performance Considerations ===" << endl;
    cout << "\n1. Work Group Size Optimization:" << endl;
    cout << "   - 16x16 is a good default for most GPUs" << endl;
    cout << "   - Consider GPU warp/wavefront size (typically 32 or 64)" << endl;
    cout << "   - Total work group size should be multiple of warp size" << endl;
    
    cout << "\n2. Memory Access Patterns:" << endl;
    cout << "   - Coalesced memory access for better performance" << endl;
    cout << "   - Use shared memory for repeated accesses" << endl;
    cout << "   - Consider cache locality" << endl;
    
    cout << "\n3. Image Format Considerations:" << endl;
    cout << "   - rgba8 for standard 8-bit images" << endl;
    cout << "   - rgba16f for HDR processing" << endl;
    cout << "   - rgba32f for maximum precision" << endl;
    
    cout << "\n=== Other Image Processing Examples ===" << endl;
    cout << "\nEdge Detection:" << endl;
    cout << "  - Sobel filter" << endl;
    cout << "  - Canny edge detection" << endl;
    cout << "  - Work group size: 16x16" << endl;
    
    cout << "\nColor Adjustment:" << endl;
    cout << "  - Brightness/Contrast" << endl;
    cout << "  - Hue/Saturation" << endl;
    cout << "  - Work group size: 16x16 or 32x32" << endl;
    
    cout << "\nAdvanced Effects:" << endl;
    cout << "  - Bloom effect (requires multiple passes)" << endl;
    cout << "  - Depth of field" << endl;
    cout << "  - Motion blur" << endl;
    
    cout << "\n=== Summary ===" << endl;
    cout << "✓ 2D image processing with compute shaders" << endl;
    cout << "✓ Gaussian blur implementation" << endl;
    cout << "✓ Image descriptor usage" << endl;
    cout << "✓ Performance optimization tips" << endl;
    cout << "\nCompute shaders provide significant performance benefits for" << endl;
    cout << "image processing compared to traditional CPU implementations!" << endl;
    
    return 0;
}
