SET(GUI_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/gui)

SET(THEME_SOURCE    ${GUI_INCLUDE_PATH}/ThemeForm.h
                    ${GUI_INCLUDE_PATH}/ThemeEngine.h
                    ThemeEngine.cpp
                    ThemeForm.cpp)

SET(DEFAULT_THEME_SOURCE    DefaultThemeEngine.h
                            DefaultThemeEngine.cpp
                            DefaultThemeForm.h
                            DefaultThemeForm.cpp)

set(WIDGET_SOURCE   ${GUI_INCLUDE_PATH}/Widget.h
                    ${GUI_INCLUDE_PATH}/Layout.h
                    ${GUI_INCLUDE_PATH}/Form.h
                    ${GUI_INCLUDE_PATH}/Panel.h
                    Widget.cpp
                    Form.cpp
                    Panel.cpp)

SOURCE_GROUP("Widget" FILES ${WIDGET_SOURCE})
SOURCE_GROUP("Theme" FILES ${THEME_SOURCE})
SOURCE_GROUP("Theme\\Default" FILES ${DEFAULT_THEME_SOURCE})

add_cm_library(ULRE.GUI "ULRE"  ${THEME_SOURCE}
                                ${DEFAULT_THEME_SOURCE}
                                ${WIDGET_SOURCE})
