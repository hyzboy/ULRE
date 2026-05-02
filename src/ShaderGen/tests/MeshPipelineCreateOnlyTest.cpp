// MeshPipelineCreateOnlyTest is a Phase 3 gate test.
// It validates that a minimal Task/Mesh/Fragment graphics pipeline can be
// created successfully on the current machine without switching draw paths.

#include "../GLSLCompiler.h"

#include <hgl/common/ShaderStageDef.h>
#include <hgl/shadergen/GLSLCompilerConfig.h>
#include <hgl/shadergen/device/DeviceProfileTargetVersion.h>

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace hgl::graph;

namespace
{
    struct SelectedPhysicalDevice
    {
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        uint32_t graphics_queue_family = 0;
    };

    static const char *kTaskShaderSource = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
    EmitMeshTasksEXT(1u, 1u, 1u);
}
)GLSL";

    static const char *kMeshShaderSource = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;
layout(location = 0) out vec3 outColor[];

void main()
{
    SetMeshOutputsEXT(3u, 1u);

    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
    gl_MeshVerticesEXT[1].gl_Position = vec4( 1.0, -1.0, 0.0, 1.0);
    gl_MeshVerticesEXT[2].gl_Position = vec4( 0.0,  1.0, 0.0, 1.0);

    outColor[0] = vec3(1.0, 0.0, 0.0);
    outColor[1] = vec3(0.0, 1.0, 0.0);
    outColor[2] = vec3(0.0, 0.0, 1.0);

    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);
}
)GLSL";

    static const char *kFragmentShaderSource = R"GLSL(
#version 460
layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor, 1.0);
}
)GLSL";

    static bool HasDeviceExtension(VkPhysicalDevice pd, const char *ext_name)
    {
        uint32_t count = 0;
        if (vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
            return false;

        std::vector<VkExtensionProperties> props(count);
        if (vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, props.data()) != VK_SUCCESS)
            return false;

        for (const auto &p : props)
        {
            if (std::strcmp(p.extensionName, ext_name) == 0)
                return true;
        }

        return false;
    }

    static bool FindGraphicsQueueFamily(VkPhysicalDevice pd, uint32_t &out_queue_family)
    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, nullptr);
        if (count == 0)
            return false;

        std::vector<VkQueueFamilyProperties> queue_props(count);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, queue_props.data());

        for (uint32_t i = 0; i < count; ++i)
        {
            if (queue_props[i].queueCount > 0
             && (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                out_queue_family = i;
                return true;
            }
        }

        return false;
    }

    static bool PickMeshCapablePhysicalDevice(VkInstance instance, SelectedPhysicalDevice &out_selected)
    {
        uint32_t count = 0;
        if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0)
            return false;

        std::vector<VkPhysicalDevice> devices(count);
        if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) != VK_SUCCESS)
            return false;

        for (VkPhysicalDevice pd : devices)
        {
            uint32_t queue_family = 0;
            if (!FindGraphicsQueueFamily(pd, queue_family))
                continue;

            if (!HasDeviceExtension(pd, VK_EXT_MESH_SHADER_EXTENSION_NAME))
                continue;

            VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features{};
            mesh_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &mesh_features;

            vkGetPhysicalDeviceFeatures2(pd, &features2);

            if (mesh_features.meshShader != VK_TRUE || mesh_features.taskShader != VK_TRUE)
                continue;

            out_selected.physical_device = pd;
            out_selected.graphics_queue_family = queue_family;
            return true;
        }

        return false;
    }

    static VkFormat PickColorAttachmentFormat(VkPhysicalDevice pd)
    {
        static const VkFormat kCandidates[] = {
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        };

        for (VkFormat f : kCandidates)
        {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(pd, f, &props);
            if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0)
                return f;
        }

        return VK_FORMAT_UNDEFINED;
    }

    static SPVData *CompileOrFail(const uint32_t stage, const char *label, const char *source)
    {
        SPVData *spv = CompileShader(stage, source);
        if (!spv || !spv->spv_data || spv->spv_length == 0)
        {
            std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] %s compile failed.\n", label);
            if (spv)
                FreeSPVData(spv);
            return nullptr;
        }

        return spv;
    }

    static VkShaderModule CreateModule(VkDevice device, const SPVData *spv)
    {
        if (!device || !spv || !spv->spv_data || spv->spv_length == 0)
            return VK_NULL_HANDLE;

        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = spv->spv_length;
        ci.pCode = reinterpret_cast<const uint32_t *>(spv->spv_data);

        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        return module;
    }

    static void SetCompilerTargetToVulkan13()
    {
        using namespace hgl::graph::mtl::contract;

        PhysicalDeviceProfileLite profile;
        profile.name = "MeshPipelineCreateOnlyTest";
        profile.target_vulkan_version = MakeVkVersion(1, 3);
        profile.target_spv_version = SPV_VERSION_1_6;

        SetShaderCompilerPhysicalDeviceProfile(profile);
    }
}

int main()
{
    if (!InitShaderCompiler())
    {
        std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] InitShaderCompiler failed.\n");
        return 1;
    }

    SetCompilerTargetToVulkan13();

    SPVData *task_spv = CompileOrFail(static_cast<uint32_t>(ShaderStage::Task), "Task", kTaskShaderSource);
    SPVData *mesh_spv = CompileOrFail(static_cast<uint32_t>(ShaderStage::Mesh), "Mesh", kMeshShaderSource);
    SPVData *frag_spv = CompileOrFail(static_cast<uint32_t>(ShaderStage::Fragment), "Fragment", kFragmentShaderSource);

    if (!task_spv || !mesh_spv || !frag_spv)
    {
        if (task_spv) FreeSPVData(task_spv);
        if (mesh_spv) FreeSPVData(mesh_spv);
        if (frag_spv) FreeSPVData(frag_spv);
        CloseShaderCompiler();
        return 1;
    }

    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkShaderModule task_module = VK_NULL_HANDLE;
    VkShaderModule mesh_module = VK_NULL_HANDLE;
    VkShaderModule frag_module = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    SelectedPhysicalDevice selected{};

    int exit_code = 1;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "MeshPipelineCreateOnlyTest";
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;

    if (vkCreateInstance(&instance_ci, nullptr, &instance) != VK_SUCCESS)
    {
        std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] vkCreateInstance failed.\n");
        goto cleanup;
    }

    if (!PickMeshCapablePhysicalDevice(instance, selected))
    {
        std::fprintf(stderr,
                     "[MeshPipelineCreateOnlyTest] No mesh-capable GPU found (requires VK_EXT_mesh_shader + task/mesh features).\n");
        goto cleanup;
    }

    {
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue_ci{};
        queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.queueFamilyIndex = selected.graphics_queue_family;
        queue_ci.queueCount = 1;
        queue_ci.pQueuePriorities = &priority;

        VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features{};
        mesh_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        mesh_features.taskShader = VK_TRUE;
        mesh_features.meshShader = VK_TRUE;

        const char *exts[] = { VK_EXT_MESH_SHADER_EXTENSION_NAME };

        VkDeviceCreateInfo device_ci{};
        device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_ci.pNext = &mesh_features;
        device_ci.queueCreateInfoCount = 1;
        device_ci.pQueueCreateInfos = &queue_ci;
        device_ci.enabledExtensionCount = 1;
        device_ci.ppEnabledExtensionNames = exts;

        if (vkCreateDevice(selected.physical_device, &device_ci, nullptr, &device) != VK_SUCCESS)
        {
            std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] vkCreateDevice failed.\n");
            goto cleanup;
        }
    }

    task_module = CreateModule(device, task_spv);
    mesh_module = CreateModule(device, mesh_spv);
    frag_module = CreateModule(device, frag_spv);

    if (!task_module || !mesh_module || !frag_module)
    {
        std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] vkCreateShaderModule failed.\n");
        goto cleanup;
    }

    {
        const VkFormat color_format = PickColorAttachmentFormat(selected.physical_device);
        if (color_format == VK_FORMAT_UNDEFINED)
        {
            std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] No suitable color attachment format found.\n");
            goto cleanup;
        }

        VkAttachmentDescription color_attachment{};
        color_attachment.format = color_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_ref{};
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;

        VkRenderPassCreateInfo rp_ci{};
        rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_ci.attachmentCount = 1;
        rp_ci.pAttachments = &color_attachment;
        rp_ci.subpassCount = 1;
        rp_ci.pSubpasses = &subpass;

        if (vkCreateRenderPass(device, &rp_ci, nullptr, &render_pass) != VK_SUCCESS)
        {
            std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] vkCreateRenderPass failed.\n");
            goto cleanup;
        }
    }

    {
        VkPipelineLayoutCreateInfo layout_ci{};
        layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        if (vkCreatePipelineLayout(device, &layout_ci, nullptr, &pipeline_layout) != VK_SUCCESS)
        {
            std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] vkCreatePipelineLayout failed.\n");
            goto cleanup;
        }
    }

    {
        VkPipelineShaderStageCreateInfo stages[3]{};

        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
        stages[0].module = task_module;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
        stages[1].module = mesh_module;
        stages[1].pName = "main";

        stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[2].module = frag_module;
        stages[2].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input{};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.width = 1.0f;
        viewport.height = 1.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent.width = 1;
        scissor.extent.height = 1;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blend_attachment{};
        blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &blend_attachment;

        VkGraphicsPipelineCreateInfo gp_ci{};
        gp_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gp_ci.stageCount = 3;
        gp_ci.pStages = stages;
        gp_ci.pVertexInputState = &vertex_input;
        gp_ci.pInputAssemblyState = &input_assembly;
        gp_ci.pViewportState = &viewport_state;
        gp_ci.pRasterizationState = &rasterization;
        gp_ci.pMultisampleState = &multisample;
        gp_ci.pColorBlendState = &blend;
        gp_ci.layout = pipeline_layout;
        gp_ci.renderPass = render_pass;
        gp_ci.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp_ci, nullptr, &pipeline) != VK_SUCCESS)
        {
            std::fprintf(stderr, "[MeshPipelineCreateOnlyTest] vkCreateGraphicsPipelines failed.\n");
            goto cleanup;
        }
    }

    std::fprintf(stdout,
                 "[MeshPipelineCreateOnlyTest] PASS: Task/Mesh/Fragment graphics pipeline created successfully.\n");
    exit_code = 0;

cleanup:
    if (pipeline)         vkDestroyPipeline(device, pipeline, nullptr);
    if (pipeline_layout)  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    if (render_pass)      vkDestroyRenderPass(device, render_pass, nullptr);

    if (task_module)      vkDestroyShaderModule(device, task_module, nullptr);
    if (mesh_module)      vkDestroyShaderModule(device, mesh_module, nullptr);
    if (frag_module)      vkDestroyShaderModule(device, frag_module, nullptr);

    if (device)
        vkDestroyDevice(device, nullptr);

    if (instance)
        vkDestroyInstance(instance, nullptr);

    FreeSPVData(task_spv);
    FreeSPVData(mesh_spv);
    FreeSPVData(frag_spv);
    CloseShaderCompiler();

    return exit_code;
}
