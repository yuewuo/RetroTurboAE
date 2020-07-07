project (soxr C)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
set (DESCRIPTION_SUMMARY "High quality, one-dimensional sample-rate conversion library")
set (LIB_TYPE SHARED)
# set (LIB_TYPE STATIC)  # to avoid dll
# Release versioning:
find_package (SIMD)
set (PROJECT_VERSION_MAJOR 0)
set (PROJECT_VERSION_MINOR 1)
set (PROJECT_VERSION_PATCH 1)

# For shared-object; if, since the last public release:
#  * library code changed at all: ++revision
#  * interfaces changed at all:   ++current, revision = 0
#  * interfaces added:            ++age
#  * interfaces removed:          age = 0

set (SO_VERSION_CURRENT  1)
set (SO_VERSION_REVISION 0)
set (SO_VERSION_AGE      1)

add_definitions (${PROJECT_C_FLAGS} -DSOXR_LIB)
# Libsoxr configuration:
option (WITH_SINGLE_PRECISION "Build with single precision (for up to 20-bit accuracy)." ON)
option (BUILD_SHARED_LIBS "Build shared libraries." ON)

set (RDFT32 fft4g32)
set (RDFT32S fft4g32s)

set (SP_SOURCES rate32 ${RDFT32})

set (SIMD_SOURCES rate32s ${RDFT32S} simd)

foreach (source ${SIMD_SOURCES})
	set_property (SOURCE ${source} PROPERTY COMPILE_FLAGS ${SIMD_C_FLAGS})
endforeach ()
message(${SIMD_SOURCES})


# Libsoxr:

add_library (${PROJECT_NAME} ${LIB_TYPE} ${PROJECT_NAME}.c data-io dbesi0 filter fft4g64 ${SP_SOURCES} vr32 ${DP_SOURCES} ${SIMD_SOURCES})
