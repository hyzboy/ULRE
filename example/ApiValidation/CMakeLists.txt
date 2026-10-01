macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/ApiValidation")
endmacro()

# API / 硬件能力验证型示例：逐个验证引擎底层某条链路能否跑通，
# 多数刻意绕过 ShaderGen / ECS 生产路径（自建管线、裸 vkCmd*、自算 CPU 真值对拍），
# 属于"可用性探针"，不随正式引擎演进，故独立于 example/Basic 等示范目录。
CreateProject(SimpleMeshTriangle             SimpleMeshTriangle.cpp)
CreateProject(ComputeDispatch                ComputeDispatch.cpp)
CreateProject(ComputeTransformHierarchy      ComputeTransformHierarchy.cpp)
CreateProject(ComputeIndirectCount           ComputeIndirectCount.cpp)
CreateProject(ComputeFrustumCull             ComputeFrustumCull.cpp)
CreateProject(ComputeAsteroidBelt            ComputeAsteroidBelt.cpp)
