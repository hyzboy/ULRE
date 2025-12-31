SET(WORK_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl)

SET(WORK_SOURCE	${WORK_INCLUDE_PATH}/WorkObject.h
                ${WORK_INCLUDE_PATH}/WorkManager.h
                WorkObject.cpp
                WorkManager.cpp)

add_cm_library(ULRE.Work "ULRE" ${WORK_SOURCE})
