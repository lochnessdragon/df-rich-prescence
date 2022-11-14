cmake_minimum_required(VERSION 3.22)

# enable FetchContent
include(FetchContent)

project(rich-prescence)

# get dfhack
FetchContent_Declare(
    "DFHack"
    GIT_REPOSITORY "https://github.com/DFHack/dfhack.git"
    SOURCE_DIR "_deps/dfhack"
    GIT_TAG "origin/master"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
  )
FetchContent_MakeAvailable(DFHack)

# get discord sdk

# build dfhack

# build plugin
DFHACK_PLUGIN(rich-prescence src/main.cpp)