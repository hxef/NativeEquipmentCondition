include(${CMAKE_SOURCE_DIR}/extern/BethesdaCMakeModules/Modules/Papyrus.cmake)


file(GLOB_RECURSE Papyrus_INPUT
	CONFIGURE_DEPENDS
	LIST_DIRECTORIES false
	*.psc
)

cmake_path(APPEND CMAKE_CURRENT_SOURCE_DIR "HUDFramework.psc" OUTPUT_VARIABLE HudFrameworkFile)
if(NOT EXISTS ${HudFrameworkFile})
	message(FATAL_ERROR "${HudFrameworkFile} not found.")
endif()

list(REMOVE_ITEM Papyrus_INPUT ${HudFrameworkFile})
list(TRANSFORM Papyrus_INPUT REPLACE "${CMAKE_CURRENT_SOURCE_DIR}\/" "")
message(${Papyrus_INPUT})
set_from_environment(HxfItemDegradationScripts)

Papyrus_Add(
	"Papyrus"
	GAME
		${Fallout4Path}
	MODE
		"Fallout4"
	IMPORTS
		${CMAKE_CURRENT_SOURCE_DIR}
	SOURCES
		${Papyrus_INPUT}
	ANONYMIZE
)

source_group(
	"Scripts"
	FILES
		${Papyrus_INPUT}
)
