
FILE(GLOB_RECURSE SRC_FILES "*.cpp")
foreach (cpp ${SRC_FILES})
	string(REGEX REPLACE ".+/(.+)\\..*" "\\1" cppname ${cpp})
	add_executable(${subprefix}${cppname} ${cpp})
endforeach(cpp)

target_link_libraries(${subprefix}Preamble_Tool soxr)
target_link_libraries(${subprefix}Demodulate_Preamble_WY_190718 soxr)
target_link_libraries(${subprefix}GetRef_Preamble_WY_190718 soxr)
target_link_libraries(${subprefix}Output_Preamble_UnionCurve soxr)
target_link_libraries(${subprefix}GetRef_Preamble_PrWangAdvanced_190719 soxr)
