cmake_minimum_required(VERSION 3.18)

set(SOLUTION_NAME DFIGPluginSol)

#BEGIN common part for all natID solutions
project(${SOLUTION_NAME})
set(SOURCE_ROOT ${CMAKE_CURRENT_LIST_DIR})
if (WIN32)
	set(HOME_ROOT $ENV{USERPROFILE})
	string(REPLACE "\\" "/" HOME_ROOT "${HOME_ROOT}")
else()
	set(HOME_ROOT $ENV{HOME})
endif()
set(NATID_SDK_ROOT ${HOME_ROOT}/natID.SDK)
include(${NATID_SDK_ROOT}/DevEnv/Common.cmake) 
#END common part for all natID solutions

include(${NATID_SDK_ROOT}/DevEnv/natGUI.cmake)
include(${NATID_SDK_ROOT}/DevEnv/MatrixLib.cmake)

include(DFIGPlugin.cmake)

# Standalone test runner executable
set(TEST_RUNNER_NAME test_runner)
add_executable(${TEST_RUNNER_NAME} 
    ${CMAKE_CURRENT_LIST_DIR}/test_runner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ModelConverter.cpp
    ${DFIGPLUGIN_CPP_INCS}
    ${DFIGPLUGIN_INC_TD}
    ${DFIGPLUGIN_INC_SYST}
    ${DFIGPLUGIN_INC_CNT}
    ${DFIGPLUGIN_INC_MU}
    ${DFIGPLUGIN_INC_MEM}
    ${DFIGPLUGIN_INC_FO}
    ${DFIGPLUGIN_INC_SC}
    ${DFIGPLUGIN_INC_DENSE}
    ${DFIGPLUGIN_INC_SPARSE}
)

target_link_libraries(${TEST_RUNNER_NAME} 
    debug ${MU_LIB_DEBUG} optimized ${MU_LIB_RELEASE}
    debug ${MATRIX_LIB_DEBUG} optimized ${MATRIX_LIB_RELEASE}
    debug ${NATGUI_LIB_DEBUG} optimized ${NATGUI_LIB_RELEASE}
)

