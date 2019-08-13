#include<hgl/graph/shader/glsl2spv.h>
#include<SPIRV/GlslangToSpv.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            static TBuiltInResource default_build_in_resource;

            void init_default_build_in_resource() 
            {
                default_build_in_resource.maxLights = 32;
                default_build_in_resource.maxClipPlanes = 6;
                default_build_in_resource.maxTextureUnits = 32;
                default_build_in_resource.maxTextureCoords = 32;
                default_build_in_resource.maxVertexAttribs = 64;
                default_build_in_resource.maxVertexUniformComponents = 4096;
                default_build_in_resource.maxVaryingFloats = 64;
                default_build_in_resource.maxVertexTextureImageUnits = 32;
                default_build_in_resource.maxCombinedTextureImageUnits = 80;
                default_build_in_resource.maxTextureImageUnits = 32;
                default_build_in_resource.maxFragmentUniformComponents = 4096;
                default_build_in_resource.maxDrawBuffers = 32;
                default_build_in_resource.maxVertexUniformVectors = 128;
                default_build_in_resource.maxVaryingVectors = 8;
                default_build_in_resource.maxFragmentUniformVectors = 16;
                default_build_in_resource.maxVertexOutputVectors = 16;
                default_build_in_resource.maxFragmentInputVectors = 15;
                default_build_in_resource.minProgramTexelOffset = -8;
                default_build_in_resource.maxProgramTexelOffset = 7;
                default_build_in_resource.maxClipDistances = 8;
                default_build_in_resource.maxComputeWorkGroupCountX = 65535;
                default_build_in_resource.maxComputeWorkGroupCountY = 65535;
                default_build_in_resource.maxComputeWorkGroupCountZ = 65535;
                default_build_in_resource.maxComputeWorkGroupSizeX = 1024;
                default_build_in_resource.maxComputeWorkGroupSizeY = 1024;
                default_build_in_resource.maxComputeWorkGroupSizeZ = 64;
                default_build_in_resource.maxComputeUniformComponents = 1024;
                default_build_in_resource.maxComputeTextureImageUnits = 16;
                default_build_in_resource.maxComputeImageUniforms = 8;
                default_build_in_resource.maxComputeAtomicCounters = 8;
                default_build_in_resource.maxComputeAtomicCounterBuffers = 1;
                default_build_in_resource.maxVaryingComponents = 60;
                default_build_in_resource.maxVertexOutputComponents = 64;
                default_build_in_resource.maxGeometryInputComponents = 64;
                default_build_in_resource.maxGeometryOutputComponents = 128;
                default_build_in_resource.maxFragmentInputComponents = 128;
                default_build_in_resource.maxImageUnits = 8;
                default_build_in_resource.maxCombinedImageUnitsAndFragmentOutputs = 8;
                default_build_in_resource.maxCombinedShaderOutputResources = 8;
                default_build_in_resource.maxImageSamples = 0;
                default_build_in_resource.maxVertexImageUniforms = 0;
                default_build_in_resource.maxTessControlImageUniforms = 0;
                default_build_in_resource.maxTessEvaluationImageUniforms = 0;
                default_build_in_resource.maxGeometryImageUniforms = 0;
                default_build_in_resource.maxFragmentImageUniforms = 8;
                default_build_in_resource.maxCombinedImageUniforms = 8;
                default_build_in_resource.maxGeometryTextureImageUnits = 16;
                default_build_in_resource.maxGeometryOutputVertices = 256;
                default_build_in_resource.maxGeometryTotalOutputComponents = 1024;
                default_build_in_resource.maxGeometryUniformComponents = 1024;
                default_build_in_resource.maxGeometryVaryingComponents = 64;
                default_build_in_resource.maxTessControlInputComponents = 128;
                default_build_in_resource.maxTessControlOutputComponents = 128;
                default_build_in_resource.maxTessControlTextureImageUnits = 16;
                default_build_in_resource.maxTessControlUniformComponents = 1024;
                default_build_in_resource.maxTessControlTotalOutputComponents = 4096;
                default_build_in_resource.maxTessEvaluationInputComponents = 128;
                default_build_in_resource.maxTessEvaluationOutputComponents = 128;
                default_build_in_resource.maxTessEvaluationTextureImageUnits = 16;
                default_build_in_resource.maxTessEvaluationUniformComponents = 1024;
                default_build_in_resource.maxTessPatchComponents = 120;
                default_build_in_resource.maxPatchVertices = 32;
                default_build_in_resource.maxTessGenLevel = 64;
                default_build_in_resource.maxViewports = 16;
                default_build_in_resource.maxVertexAtomicCounters = 0;
                default_build_in_resource.maxTessControlAtomicCounters = 0;
                default_build_in_resource.maxTessEvaluationAtomicCounters = 0;
                default_build_in_resource.maxGeometryAtomicCounters = 0;
                default_build_in_resource.maxFragmentAtomicCounters = 8;
                default_build_in_resource.maxCombinedAtomicCounters = 8;
                default_build_in_resource.maxAtomicCounterBindings = 1;
                default_build_in_resource.maxVertexAtomicCounterBuffers = 0;
                default_build_in_resource.maxTessControlAtomicCounterBuffers = 0;
                default_build_in_resource.maxTessEvaluationAtomicCounterBuffers = 0;
                default_build_in_resource.maxGeometryAtomicCounterBuffers = 0;
                default_build_in_resource.maxFragmentAtomicCounterBuffers = 1;
                default_build_in_resource.maxCombinedAtomicCounterBuffers = 1;
                default_build_in_resource.maxAtomicCounterBufferSize = 16384;
                default_build_in_resource.maxTransformFeedbackBuffers = 4;
                default_build_in_resource.maxTransformFeedbackInterleavedComponents = 64;
                default_build_in_resource.maxCullDistances = 8;
                default_build_in_resource.maxCombinedClipAndCullDistances = 8;
                default_build_in_resource.maxSamples = 4;
                default_build_in_resource.maxMeshOutputVerticesNV = 256;
                default_build_in_resource.maxMeshOutputPrimitivesNV = 512;
                default_build_in_resource.maxMeshWorkGroupSizeX_NV = 32;
                default_build_in_resource.maxMeshWorkGroupSizeY_NV = 1;
                default_build_in_resource.maxMeshWorkGroupSizeZ_NV = 1;
                default_build_in_resource.maxTaskWorkGroupSizeX_NV = 32;
                default_build_in_resource.maxTaskWorkGroupSizeY_NV = 1;
                default_build_in_resource.maxTaskWorkGroupSizeZ_NV = 1;
                default_build_in_resource.maxMeshViewCountNV = 4;
                default_build_in_resource.limits.nonInductiveForLoops = 1;
                default_build_in_resource.limits.whileLoops = 1;
                default_build_in_resource.limits.doWhileLoops = 1;
                default_build_in_resource.limits.generalUniformIndexing = 1;
                default_build_in_resource.limits.generalAttributeMatrixVectorIndexing = 1;
                default_build_in_resource.limits.generalVaryingIndexing = 1;
                default_build_in_resource.limits.generalSamplerIndexing = 1;
                default_build_in_resource.limits.generalVariableIndexing = 1;
                default_build_in_resource.limits.generalConstantMatrixVectorIndexing = 1;
            }

            EShLanguage FindLanguage(const VkShaderStageFlagBits shader_type) 
            {
                switch (shader_type) 
                {
                    case VK_SHADER_STAGE_VERTEX_BIT:
                        return EShLangVertex;

                    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                        return EShLangTessControl;

                    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                        return EShLangTessEvaluation;

                    case VK_SHADER_STAGE_GEOMETRY_BIT:
                        return EShLangGeometry;

                    case VK_SHADER_STAGE_FRAGMENT_BIT:
                        return EShLangFragment;

                    case VK_SHADER_STAGE_COMPUTE_BIT:
                        return EShLangCompute;

                    default:
                        return EShLangVertex;
                }
            }
        }//namespace

        void InitDefaultShaderBuildResource()
        {
            init_default_build_in_resource();
        }

        bool GLSL2SPV(const VkShaderStageFlagBits shader_type,const char *shader_source,std::vector<uint32> &spirv,UTF8String &log,UTF8String &debug_log)
        {
            EShLanguage stage = FindLanguage(shader_type);

            glslang::TShader shader(stage);
            glslang::TProgram program;

            const char *shaderStrings[1];
            TBuiltInResource Resources=default_build_in_resource;

            // Enable SPIR-V and Vulkan rules when parsing GLSL
            EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

            shaderStrings[0] = shader_source;
            shader.setStrings(shaderStrings, 1);

            if (!shader.parse(&Resources, 100, false, messages)) 
            {
                puts(shader.getInfoLog());
                puts(shader.getInfoDebugLog());
                return false;  // something didn't work
            }

            program.addShader(&shader);

            //
            // Program-level processing...
            //

            if (!program.link(messages)) 
            {
                puts(shader.getInfoLog());
                puts(shader.getInfoDebugLog());
                fflush(stdout);
                return false;
            }

            glslang::GlslangToSpv(*program.getIntermediate(stage),spirv);
            return(true);
        }
    }//namespace graph
}//namespace hgl
