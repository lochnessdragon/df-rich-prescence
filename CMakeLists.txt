cmake_minimum_required(VERSION 3.16)

# enable FetchContent
include(FetchContent)

project(rich_presence)

# get discord sdk (https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip)
# cmake_policy(SET CMP0135 NEW)
FetchContent_Declare(
  discordSDK 
  URL "https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip"
  URL_MD5 73e5e1b3f8413a2c7184ef17476822f2
  #CONFIGURE_COMMAND "ln -s lib/x86_64/discord_game_sdk.so lib/x86_64/libdiscord_game_sdk.so"
	#DOWNLOAD_EXTRACT_TIMESTAMP true
	BUILD_COMMAND ""
)
FetchContent_MakeAvailable(discordSDK)
set(DISCORD_SDK_DLL ${discordsdk_SOURCE_DIR}/lib/x86_64/discord_game_sdk.so)

# build discord sdk stuff
file(GLOB SDK_SOURCES ${discordsdk_SOURCE_DIR}/cpp/*.cpp)
add_library(discord_sdk STATIC ${SDK_SOURCES})
link_directories(${discordsdk_SOURCE_DIR}/lib/x86_64/)
IF(WIN32)
	target_link_libraries(discord_sdk discord_game_sdk.dll.lib)
	set(PLUGIN_LIBS discord_sdk)
	# message(INFO "Targeting windows")
ELSE()
	target_link_libraries(discord_sdk discord_game_sdk)
	set(PLUGIN_LIBS discord_sdk discord_game_sdk)
ENDIF()
# build plugin
DFHACK_PLUGIN(rich_presence src/main.cpp LINK_LIBRARIES ${PLUGIN_LIBS})

# read in variables from file
# file(READ .env ENV_FILE)
# message(STATUS ${ENV_FILE})
# add_compile_definitions(${ENV_FILE})

IF(NOT WIN32)
  # create symlink to fix discord_game_sdk.so on linux like classrooms
  add_custom_command(TARGET rich_presence PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${discordsdk_SOURCE_DIR}/lib/x86_64/discord_game_sdk.so ${discordsdk_SOURCE_DIR}/lib/x86_64/libdiscord_game_sdk.so)
ENDIF()

# add discord sdk include headers (todo: remove rpath)
target_include_directories(rich_presence PUBLIC ${discordsdk_SOURCE_DIR}/)

# install the discord sdk dll as well when running the cmake install command
install(FILES ${DISCORD_SDK_DLL}
        DESTINATION ${DFHACK_BINARY_DESTINATION}
        RENAME libdiscord_game_sdk.so)