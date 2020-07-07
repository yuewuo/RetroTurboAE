cmake_minimum_required(VERSION 3.7)
project(RetroTurbo C CXX)

# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)


if (WIN32)
	set(CMAKE_EXE_LINKER_FLAGS "-Wl,--stack,16777216")#windows
	set(CMAKE_C_FLAGS_DEBUG "-g -Wall -Wextra -Wshadow -O2 -fsigned-char")
	set(CMAKE_CXX_FLAGS_DEBUG "-g2 -Wall -Wextra -Wshadow -O0 -fsigned-char -march=haswell")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -funroll-loops -Wall -Wextra -Wshadow")
	set(CMAKE_C_FLAGS_RELEASE "-O2 -funroll-loops -Wall -Wextra -Wshadow")
else()
	set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,stack-size=16777216")#linux
	set(CMAKE_C_FLAGS_DEBUG "-g -Wall -Wextra -Wshadow -O2 -fsigned-char -fPIC")
	set(CMAKE_CXX_FLAGS_DEBUG "-g2 -Wall -Wextra -Wshadow -O0 -fsigned-char -march=haswell -fPIC")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -funroll-loops -Wall -Wextra -Wshadow -fPIC")
	set(CMAKE_C_FLAGS_RELEASE "-O2 -funroll-loops -Wall -Wextra -Wshadow -fPIC")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -Wshadow")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -funroll-loops -Wall -Wextra -Wshadow")

macro (Link _name)
	if (TARGET ${_name})
		if (APPLE) 
			target_link_libraries(${_name} "-framework CoreFoundation")
			target_link_libraries(${_name} "-framework IOKit")
			set_target_properties(${_name} PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")
		elseif (WIN32)
			target_link_libraries(${_name} setupapi hid uuid)
		endif()
		target_link_libraries(${_name} pthread mosquitto mongoc bson)
	endif()
endmacro()

if (UNIX)
	link_libraries(dl m readline)
endif()

include_directories(${PROJECT_SOURCE_DIR}/RetroLib/)

include_directories(${PROJECT_SOURCE_DIR}/RetroLib/restrpc/)
include_directories(${PROJECT_SOURCE_DIR}/RetroLib/restrpc/asio-1.12.2/include)
include_directories(${PROJECT_SOURCE_DIR}/RetroLib/restrpc/msgpack/include)

include_directories(${PROJECT_SOURCE_DIR}/SoftReader/)
include_directories(${PROJECT_SOURCE_DIR}/SoftTag/)

# optimize order for less compile time
add_subdirectory("RetroLib/soxr")
add_library(preamble SHARED RetroLib/preamble.cpp)
target_link_libraries(preamble soxr)
add_subdirectory("RetroLib/lua")
file(GLOB serial_cc . RetroLib/serial/*.cc)
add_library(serial STATIC ${serial_cc})
Link(serial)

# use quirc QRcode library
set(QDIR RetroLib/quirc/lib)
include_directories(${QDIR})
add_library(quirc STATIC ${QDIR}/decode.c ${QDIR}/identify.c ${QDIR}/quirc.c ${QDIR}/version_db.c)

link_libraries(quirc)
link_libraries(liblua)
link_libraries(serial)

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})
set(CMAKE_INSTALL_BINDIR "")
set(CMAKE_INSTALL_LIBDIR "")

set(subprefix "DT_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/DebugTest)
set(subprefix "T4_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/TagL4)
set(subprefix "HD_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/HandleData)
#set(subprefix "AE_")
#add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/AutoExperiment)
set(subprefix "AR_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/Archive)
set(subprefix "EL_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/ExploreLCD)
set(subprefix "EM_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/Emulation)
set(subprefix "ML_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/MatLab)

# below is the subdirectories that relay on libpreamble
link_libraries(preamble)

set(subprefix "R7_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/ReaderH7)
set(subprefix "TH_")
add_subdirectory(${PROJECT_SOURCE_DIR}/Tester/TurboHost)

MESSAGE(STATUS "you need to copy RetroLib/soxr/soxr.dll here to run program on Windows")
