cmake_minimum_required(VERSION 3.7)
project (lua C)

FILE(GLOB_RECURSE SRC_FILES "*.c")
# message (SRC_FILES=${SRC_FILES})
add_library(liblua STATIC ${SRC_FILES})
add_executable(lua lua.c)
target_link_libraries(lua liblua)
add_executable(luac luac.c)
target_link_libraries(luac liblua)

if (UNIX)
    add_definitions(-DLUA_USE_LINUX)
endif()
