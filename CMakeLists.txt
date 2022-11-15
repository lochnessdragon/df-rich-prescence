cmake_minimum_required(VERSION 3.16)

# enable FetchContent
include(FetchContent)

project(rich_prescence)

# get discord sdk (https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip)

# build plugin
DFHACK_PLUGIN(rich_prescence src/main.cpp)