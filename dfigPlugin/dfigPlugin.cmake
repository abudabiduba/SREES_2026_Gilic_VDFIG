set(DFIGPLUGIN_NAME dfigPlugin)				#Naziv prvog projekta u solution-u

if (NOT COMMAND setIDEPropertiesForLib)
    function(setIDEPropertiesForLib targetID)
    endfunction()
endif()

file(GLOB DFIGPLUGIN_CPP_COMMON_SOURCES  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB DFIGPLUGIN_CPP_COMMON_INCS  ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
file(GLOB DFIGPLUGIN_INC_GUI  ${NATID_SDK_INC}/gui/*.h)
file(GLOB DFIGPLUGIN_INC_TD  ${NATID_SDK_INC}/td/*.h)
file(GLOB DFIGPLUGIN_INC_CNT  ${NATID_SDK_INC}/cnt/*.h)
file(GLOB DFIGPLUGIN_INC_MU  ${NATID_SDK_INC}/mu/*.h)
file(GLOB DFIGPLUGIN_INC_MEM  ${NATID_SDK_INC}/mem/*.h)
file(GLOB DFIGPLUGIN_INC_FO ${NATID_SDK_INC}/fo/*.h)
file(GLOB DFIGPLUGIN_INC_SC ${NATID_SDK_INC}/sc/*.h)
file(GLOB DFIGPLUGIN_INC_SYST ${NATID_SDK_INC}/syst/*.h)
file(GLOB DFIGPLUGIN_INC_DENSE ${NATID_SDK_INC}/dense/*.h)
file(GLOB DFIGPLUGIN_INC_SPARSE ${NATID_SDK_INC}/sparse/*.h)

# add shared library (plugin is a shared executatable binary file)
add_library(${DFIGPLUGIN_NAME} SHARED ${DFIGPLUGIN_CPP_COMMON_SOURCES} ${DFIGPLUGIN_INC_GUI} ${DFIGPLUGIN_CPP_COMMON_INCS} 
							${DFIGPLUGIN_INC_TD} ${DFIGPLUGIN_INC_SYST} 
							${DFIGPLUGIN_INC_CNT} ${DFIGPLUGIN_INC_MU} ${DFIGPLUGIN_INC_MEM} ${DFIGPLUGIN_INC_FO}
							${DFIGPLUGIN_INC_SC} ${DFIGPLUGIN_INC_DENSE} ${DFIGPLUGIN_INC_SPARSE})

source_group("inc\\inc"        FILES ${DFIGPLUGIN_CPP_COMMON_INCS})
source_group("inc\\gui"        FILES ${DFIGPLUGIN_INC_GUI})
source_group("inc\\td"        FILES ${DFIGPLUGIN_INC_TD})
source_group("inc\\cnt"        FILES ${DFIGPLUGIN_INC_CNT})
source_group("inc\\dense"        FILES ${DFIGPLUGIN_INC_DENSE})
source_group("inc\\mu"        FILES ${DFIGPLUGIN_INC_MU})
source_group("inc\\mem"        FILES ${DFIGPLUGIN_INC_MEM})
source_group("inc\\fo"        FILES ${DFIGPLUGIN_INC_FO})
source_group("inc\\sc"        FILES ${DFIGPLUGIN_INC_SC})
source_group("inc\\sparse"        FILES ${DFIGPLUGIN_INC_SPARSE})
source_group("inc\\syst"        FILES ${DFIGPLUGIN_INC_SYST})

source_group("src\\cpp"			FILES ${DFIGPLUGIN_CPP_COMMON_SOURCES})

target_link_libraries(${DFIGPLUGIN_NAME} debug ${MU_LIB_DEBUG} optimized ${MU_LIB_RELEASE} 
										debug ${MATRIX_LIB_DEBUG} optimized ${MATRIX_LIB_RELEASE}
									  debug ${NATGUI_LIB_DEBUG} optimized ${NATGUI_LIB_RELEASE})
									
target_compile_definitions(${DFIGPLUGIN_NAME} PUBLIC PLUGIN_EXPORTS)

setIDEPropertiesForLib(${DFIGPLUGIN_NAME})
