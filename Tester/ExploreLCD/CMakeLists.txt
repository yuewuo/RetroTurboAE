
FILE(GLOB_RECURSE SRC_FILES "*.cpp")
foreach (cpp ${SRC_FILES})
	string(REGEX REPLACE ".+/(.+)\\..*" "\\1" cppname ${cpp})
	add_executable(${subprefix}${cppname} ${cpp})
endforeach(cpp)

target_link_libraries(EL_AutoGenRef8421_HandleOne soxr)
target_link_libraries(EL_200118_PreGenerateRef17Mseq soxr)
