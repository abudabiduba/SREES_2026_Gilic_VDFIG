set(TESTRUNNER_NAME test_runner)		

file(GLOB TESTRUNNER_SOURCES  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB TESTRUNNER_INCS  ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
set(TESTRUNNER_PLIST  ${CMAKE_CURRENT_LIST_DIR}/src/Info.plist)
file(GLOB TESTRUNNER_INC_TD  ${NATID_SDK_INC}/td/*.h)
file(GLOB TESTRUNNER_INC_GUI ${NATID_SDK_INC}/gui/*.h)
file(GLOB TESTRUNNER_INC_SYST ${NATID_SDK_INC}/syst/*.h)
file(GLOB TESTRUNNER_INC_SC ${NATID_SDK_INC}/sc/*.h)
file(GLOB TESTRUNNER_INC_FO ${NATID_SDK_INC}/fo/*.h)
file(GLOB TESTRUNNER_INC_ARCH ${NATID_SDK_INC}/arch/*.h)

# add executable
add_executable(${TESTRUNNER_NAME} ${TESTRUNNER_INCS} ${TESTRUNNER_SOURCES} ${TESTRUNNER_INC_TD} 
			${TESTRUNNER_INC_GUI} ${TESTRUNNER_INC_SYST} ${TESTRUNNER_INC_SC} ${TESTRUNNER_INC_FO} ${TESTRUNNER_INC_ARCH} )

source_group("inc"            FILES ${TESTRUNNER_INCS})
source_group("inc\\td"        FILES ${TESTRUNNER_INC_TD})
source_group("inc\\gui"        FILES ${TESTRUNNER_INC_GUI})
source_group("inc\\arch"        FILES ${TESTRUNNER_INC_ARCH})
source_group("inc\\fo"        FILES ${TESTRUNNER_INC_FO})
source_group("inc\\sc"        FILES ${TESTRUNNER_INC_SC})
source_group("inc\\syst"        FILES ${TESTRUNNER_INC_SYST})

source_group("src"            FILES ${TESTRUNNER_SOURCES})

target_link_libraries(${TESTRUNNER_NAME} debug ${MU_LIB_DEBUG} debug ${NATGUI_LIB_DEBUG} 
										optimized ${MU_LIB_RELEASE} optimized ${NATGUI_LIB_RELEASE})

set(PLUGIN_DIR_LOG "$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>$<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>")

# Pass it as a macro definition to the application code
target_compile_definitions(${TESTRUNNER_NAME} PRIVATE 
    PLUGIN_DIR="${PLUGIN_DIR_LOG}"
)

setTargetPropertiesForGUIApp(${TESTRUNNER_NAME} ${TESTRUNNER_PLIST})

setIDEPropertiesForGUIExecutable(${TESTRUNNER_NAME} ${CMAKE_CURRENT_LIST_DIR})

setPlatformDLLPath(${TESTRUNNER_NAME})
