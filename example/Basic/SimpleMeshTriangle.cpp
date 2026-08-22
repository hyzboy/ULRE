// P1: MeshShader 最小验证示例——手写 mesh+frag GLSL，裸 vkCreateGraphicsPipelines 建 mesh 管线，
// vkCmdDrawMeshTasksEXT 出图。验证 VK_EXT_mesh_shader 全链（编译→管线→绘制）在 AMD 6700 XT 上可用。
//
// 注意：本示例故意绕过 ShaderGen/ECS——P1 目标是打通 mesh shader 硬链路的 smoke test。
// mesh 管线无 vertex input（pVertexInputState/pInputAssemblyState = nullptr），顶点/图元数据在 mesh shader 内生成。

#include<hgl/framework/WorkManager.h>
#include<hgl/framework/WorkObject.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/log/Log.h>
#include<hgl/ShaderCompilerAPI.h>     // hgl::graph::CompileShader / InitShaderCompiler（加载 D:\GLSLCompiler.dll）

using namespace hgl;
using namespace hgl::graph;

namespace
{
    // 最小 mesh shader：1 个 threadgroup，输出 1 个三角形（3 顶点 1 图元）
    const char *MESH_GLSL = R"GLSL(
#version 450
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;

void main()
{
    SetMeshOutputsEXT(3, 1);

    gl_MeshVerticesEXT[0].gl_Position = vec4(-0.8, -0.8, 0.0, 1.0);
    gl_MeshVerticesEXT[1].gl_Position = vec4( 0.8, -0.8, 0.0, 1.0);
    gl_MeshVerticesEXT[2].gl_Position = vec4( 0.0,  0.8, 0.0, 1.0);

    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
}
)GLSL";

    const char *FRAG_GLSL = R"GLSL(
#version 450

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(1.0, 0.3, 0.2, 1.0);
}
)GLSL";
}

class TestApp:public WorkObject
{
    VkPipeline mesh_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout mesh_layout = VK_NULL_HANDLE;
    ShaderModule *mesh_module = nullptr;
    ShaderModule *frag_module = nullptr;

    ShaderModule *CreateModule(const VkShaderStageFlagBits stage,const char *source)
    {
        auto *device = GetDevice();
        if(!device)
            return nullptr;

        SPVData *spv = CompileShader(static_cast<uint32_t>(stage),source);
        if(!spv)
        {
            GLogError(u8"编译失败 stage=%u（GLSLCompiler 是否已重建为强制 1.4？）",uint(stage));
            return nullptr;
        }

        ShaderModule *module=device->CreateShaderModule(stage,spv->spv_data,spv->spv_length);
        FreeSPVData(spv);

        return module;
    }

    bool CreateMeshPipeline()
    {
        auto *device = GetDevice();
        if(!device)
            return(false);

        if(!InitShaderCompiler())
        {
            GLogError(u8"InitShaderCompiler 失败——D:\\GLSLCompiler.dll 缺失或未重建");
            return(false);
        }

        mesh_module = CreateModule(VK_SHADER_STAGE_MESH_BIT_EXT,MESH_GLSL);
        frag_module = CreateModule(VK_SHADER_STAGE_FRAGMENT_BIT,FRAG_GLSL);

        if(!mesh_module||!frag_module)
            return(false);

        const VkPipelineShaderStageCreateInfo stages[]=
        {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_MESH_BIT_EXT,*mesh_module,"main",nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,*frag_module,"main",nullptr},
        };

        VkPipelineLayoutCreateInfo layout_ci{};
        layout_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        if(vkCreatePipelineLayout(device->GetDevice(),&layout_ci,nullptr,&mesh_layout)!=VK_SUCCESS)
            return(false);

        // mesh 管线：无 vertex input / input assembly（规范要求为 nullptr）
        VkPipelineVertexInputStateCreateInfo vi_ci{};
        vi_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo ia_ci{};
        ia_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

        const VkExtent2D *ext=GetExtent();

        VkViewport viewport{0,0,float(ext->width),float(ext->height),0,1};
        VkRect2D   scissor{{0,0},{ext->width,ext->height}};

        VkPipelineViewportStateCreateInfo vp_ci{};
        vp_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp_ci.viewportCount=1;
        vp_ci.pViewports=&viewport;
        vp_ci.scissorCount=1;
        vp_ci.pScissors=&scissor;

        VkPipelineRasterizationStateCreateInfo rs_ci{};
        rs_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs_ci.polygonMode=VK_POLYGON_MODE_FILL;
        rs_ci.cullMode=VK_CULL_MODE_NONE;
        rs_ci.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs_ci.lineWidth=1.0f;

        VkPipelineMultisampleStateCreateInfo ms_ci{};
        ms_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms_ci.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

        // render target 有 depth attachment——必须提供 depth/stencil state
        // （VUID-VkGraphicsPipelineCreateInfo-renderPass-09028）；P1 不依赖深度，测试/写入均关
        VkPipelineDepthStencilStateCreateInfo ds_ci{};
        ds_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds_ci.depthTestEnable=VK_FALSE;
        ds_ci.depthWriteEnable=VK_FALSE;

        VkPipelineColorBlendAttachmentState blend_att{};
        blend_att.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cb_ci{};
        cb_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb_ci.attachmentCount=1;
        cb_ci.pAttachments=&blend_att;

        VkGraphicsPipelineCreateInfo ci{};
        ci.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        ci.stageCount=2;
        ci.pStages=stages;
        ci.pVertexInputState=nullptr;      // mesh 管线：无 vertex input
        ci.pInputAssemblyState=nullptr;    // mesh 管线：无 input assembly
        ci.pViewportState=&vp_ci;
        ci.pRasterizationState=&rs_ci;
        ci.pMultisampleState=&ms_ci;
        ci.pDepthStencilState=&ds_ci;
        ci.pColorBlendState=&cb_ci;
        ci.layout=mesh_layout;

        // Dynamic Rendering：renderPass=VK_NULL_HANDLE + pNext 挂 VkPipelineRenderingCreateInfo
        // 声明附件格式（VUID-VkGraphicsPipelineCreateInfo-renderPass-06061）
        VkFormat color_format=VK_FORMAT_UNDEFINED;
        VkFormat depth_format=VK_FORMAT_UNDEFINED;

        if(auto *rt = GetRenderContext() ? GetRenderContext()->GetCurrentRenderTarget() : nullptr)
        {
            auto att=rt->GetColorAttachment(0);
            color_format=att.format;

            auto depth_att=rt->GetDepthAttachment();
            depth_format=depth_att.format;
        }

        if(color_format==VK_FORMAT_UNDEFINED)
        {
            GLogError(u8"无法获取 color attachment format");
            return(false);
        }

        VkPipelineRenderingCreateInfo rendering_ci{};
        rendering_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_ci.colorAttachmentCount=1;
        rendering_ci.pColorAttachmentFormats=&color_format;
        rendering_ci.depthAttachmentFormat=depth_format;

        ci.pNext=&rendering_ci;
        ci.renderPass=VK_NULL_HANDLE;
        ci.subpass=0;

        if(vkCreateGraphicsPipelines(device->GetDevice(),VK_NULL_HANDLE,1,&ci,nullptr,&mesh_pipeline)!=VK_SUCCESS)
            return(false);

        GLogInfo(u8"Mesh pipeline 创建成功（mesh+frag，无 vertex input，dynamic rendering）");
        return(true);
    }

public:

    bool Init() override
    {
        if(!CreateMeshPipeline())
            return(false);

        SetClearColor(Color4f(0.1f,0.1f,0.15f,1.0f));
        return(true);
    }

    void Render(double) override
    {
        // wo->Render 是 ECS 的 pre_render 回调——在 BeginManagedRenderFrame 的
        // BeginRenderPass（dynamic rendering）之后、ECS 系统绘制之前调用，处于 render pass 内，
        // 直接拿当前 cmd buffer 绘制即可（无需自开 render pass）
        auto *ecs=GetECSContext();
        if(!ecs)return;

        auto *cmd=ecs->GetRenderContext()->GetCurrentRenderCmdBuffer();
        if(!cmd)return;

        vkCmdBindPipeline(VkCommandBuffer(*cmd),VK_PIPELINE_BIND_POINT_GRAPHICS,mesh_pipeline);

        // 1 个 threadgroup → mesh shader 输出 1 个三角形
        cmd->DrawMeshTasks(1);
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("SimpleMeshTriangle - MeshShader P1 smoke test"),argc,argv);
}
