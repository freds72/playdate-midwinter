cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 11)

set(ENVSDK $ENV{PLAYDATE_SDK_PATH})

if (NOT ${ENVSDK} STREQUAL "")
	# Convert path from Windows
	file(TO_CMAKE_PATH ${ENVSDK} SDK)
else()
	execute_process(
			COMMAND bash -c "egrep '^\\s*SDKRoot' $HOME/.Playdate/config"
			COMMAND head -n 1
			COMMAND cut -c9-
			OUTPUT_VARIABLE SDK
			OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif()

if (NOT EXISTS ${SDK})
	message(FATAL_ERROR "SDK Path not found; set ENV value PLAYDATE_SDK_PATH")
	return()
endif()

# https://gist.github.com/Tordan/c4d4d14aac0e85ebf7122ed2189ea387
# Add command to generate version header...
add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/source/pdxinfo
        ALL
    COMMAND
        ${CMAKE_COMMAND} -D PROJECT_VERSION=${PROJECT_VERSION} -D IN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/source/pdxinfo.in -D OUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/source/pdxinfo -P ${CMAKE_CURRENT_SOURCE_DIR}/GenerateVersion.cmake
    WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
)

# ...make sure it exists in filesystem (configuration WILL fail if it's not)...
file(WRITE ${CMAKE_SOURCE_DIR}/source/pdxinfo)

# ...add custom target to rebuild pdxinfo every time
add_custom_target(
    generate_pdxinfo
        ALL
    COMMAND
        ${CMAKE_COMMAND} -D PROJECT_VERSION=${PROJECT_VERSION} -D IN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/source/pdxinfo.in -D OUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/source/pdxinfo -P ${CMAKE_CURRENT_SOURCE_DIR}/GenerateVersion.cmake
    WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

# Game Name Customization
set(PLAYDATE_GAME_NAME snow)
set(PLAYDATE_GAME_DEVICE snow_DEVICE)

project(${PLAYDATE_GAME_NAME} C ASM)

file(GLOB LIB3D_GLOB
     "lib3d/*.c"
     "lib3d/*.h"
)

file(GLOB LUA_CODE
	"source/*.lua"
)

include_directories("lib3d")

if (TOOLCHAIN STREQUAL "armgcc")
	add_executable(${PLAYDATE_GAME_DEVICE} ${LIB3D_GLOB} )
else()
	add_library(${PLAYDATE_GAME_NAME} SHARED ${LIB3D_GLOB} ${LUA_CODE})
endif()

include(${SDK}/C_API/buildsupport/playdate_game.cmake)

