cmake_minimum_required(VERSION 3.18)

set(SOLUTION_NAME TestDFIGPluginSol)

#BEGIN common part for all solutions
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
#END common part for all solutions

include(${NATID_SDK_ROOT}/DevEnv/natGUI.cmake)
include(${NATID_SDK_ROOT}/DevEnv/MatrixLib.cmake)

include(../dfigPlugin/dfigPlugin.cmake)

include(test_runner.cmake)
add_dependencies(${TESTRUNNER_NAME} ${DFIGPLUGIN_NAME}) 


