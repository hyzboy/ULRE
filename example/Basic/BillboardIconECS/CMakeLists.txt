set(BILLBOARD_ICON_BASE  BillboardIconECSBase.cpp)

CreateProject(01_Billboard                      BillboardTest.cpp)
CreateProject(02_BillboardECS                   BillboardECS.cpp)
CreateProject(03_BillboardPerspectiveECS        ${BILLBOARD_ICON_BASE}  BillboardPerspectiveECS.cpp)
CreateProject(04_BlendModeMaskedECS             ${BILLBOARD_ICON_BASE}  BlendModeMaskedECS.cpp)
CreateProject(05_BlendModeDitherECS             ${BILLBOARD_ICON_BASE}  BlendModeDitherECS.cpp)
CreateProject(06_BlendModeA2CECS                ${BILLBOARD_ICON_BASE}  BlendModeA2CECS.cpp)
CreateProject(07_BlendModeTransparentECS        ${BILLBOARD_ICON_BASE}  BlendModeTransparentECS.cpp)
CreateProject(08_DomainIsolationDemo            DomainIsolationDemo.cpp)
CreateProject(09_TextureBindingValidation       09_TextureBindingValidation.cpp)

set(BILLBOARD_TARGETS
    01_Billboard
    02_BillboardECS
    03_BillboardPerspectiveECS
    04_BlendModeMaskedECS
    05_BlendModeDitherECS
    06_BlendModeA2CECS
    07_BlendModeTransparentECS
    08_DomainIsolationDemo
    09_TextureBindingValidation
)

foreach(target_name IN LISTS BILLBOARD_TARGETS)
    set_property(TARGET ${target_name} PROPERTY FOLDER "ULRE/Example/Basic/Billboard")
endforeach()
