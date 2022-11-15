cmake_minimum_required(VERSION 3.16)

# enable FetchContent
include(FetchContent)

project(rich_presence)

# get discord sdk (https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip)
FetchContent_Declare(
    discordSDK 
    URL "https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip"
    URL_HASH MD5=73e5e1b3f8413a2c7184ef17476822f2
    #CONFIGURE_COMMAND "ln -s lib/x86_64/discord_game_sdk.so lib/x86_64/libdiscord_game_sdk.so"
)
FetchContent_MakeAvailable(discordSDK)
# build discord sdk stuff
project(discord_game_sdk)
file(GLOB SDK_SOURCES ${discordsdk_SOURCE_DIR}/cpp/*.cpp)
add_library(discord_sdk STATIC ${SDK_SOURCES})
link_directories(${discordsdk_SOURCE_DIR}/lib/x86_64/)
target_link_libraries(discord_sdk discord_game_sdk)

# build plugin
DFHACK_PLUGIN(rich_presence src/main.cpp LINK_LIBRARIES discord_sdk discord_game_sdk)

# read in variables from file
# file(READ .env ENV_FILE)
# message(STATUS ${ENV_FILE})
# add_compile_definitions(${ENV_FILE})

# create symlink to fix discord_game_sdk.so
add_custom_command(TARGET rich_presence PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${discordsdk_SOURCE_DIR}/lib/x86_64/discord_game_sdk.so ${discordsdk_SOURCE_DIR}/lib/x86_64/libdiscord_game_sdk.so)

# add discord sdk include headers (todo: remove rpath)
target_include_directories(rich_presence PUBLIC ${discordsdk_SOURCE_DIR}/)