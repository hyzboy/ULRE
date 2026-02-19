SET(WORK_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/framework)

SET(WORK_SOURCE	${WORK_INCLUDE_PATH}/WorkObject.h
                ${WORK_INCLUDE_PATH}/WorkManager.h
                ${WORK_INCLUDE_PATH}/AppFramework.h
                AppFramework.cpp
                WorkObject.cpp
                WorkManager.cpp)

add_cm_library(ULRE.Work "ULRE" ${WORK_SOURCE})
