file(GLOB GUI_HEADER ${ROOT_INCLUDE_PATH}/hgl/gui/*.*)

set(GUI_SOURCE  Widget.cpp
                Panel.cpp
                ThemeEngine.cpp
                DefaultThemeEngine.h
                DefaultThemeEngine.cpp)

SOURCE_GROUP("Header Files" FILES ${GUI_HEADER})
SOURCE_GROUP("Source Files" FILES ${GUI_SOURCE})

add_cm_library(ULRE.GUI "ULRE"  ${GUI_HEADER}
                                ${GUI_SOURCE})
