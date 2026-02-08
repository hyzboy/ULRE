set(SHADER_SCHEMA_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/shader_schema)

set(SHADER_SCHEMA_HEADER_FILES
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderSchema.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/StdMaterial.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/MaterialLibrary.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/MaterialCreateConfig.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/Material2DCreateConfig.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/Material3DCreateConfig.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderBufferSource.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/VertexAttrib.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/CoordinateSystem.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/RenderTargetOutputConfig.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/VertexInputAttribute.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/RenderAssign.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/TextureType.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/DescriptorSetType.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderStage.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderStageHelpers.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/Interpolation.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/VkTypes.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/PrimitiveType.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/SamplerName.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/UBOCommon.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/VulkanDevAttr.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderDescriptorSet.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderDescriptor.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/PhysicalDevice.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/SamplerType.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderImageType.h
    ${SHADER_SCHEMA_INCLUDE_PATH}/ShaderVariableType.h
)

set(SHADER_SCHEMA_SOURCE_FILES
    ShaderSchema.cpp
    ShaderStageHelpers.cpp
)

source_group("ShaderSchema" FILES ${SHADER_SCHEMA_HEADER_FILES} ${SHADER_SCHEMA_SOURCE_FILES})

add_cm_library(ULRE.ShaderSchema "ULRE"
    ${SHADER_SCHEMA_INCLUDE_PATH}
    ${SHADER_SCHEMA_HEADER_FILES}
    ${SHADER_SCHEMA_SOURCE_FILES}
)
