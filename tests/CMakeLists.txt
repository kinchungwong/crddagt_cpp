# This CMakeLists.txt is adapted and minimized from the following:
#    https://github.com/mortinger91/cpp-cmake-template/blob/master/projectFolder/tests/CMakeLists.txt
# 

enable_testing()

set(COMPILER_FLAGS
    -std=c++17
    -march=native
    -mtune=native
	-Og
	-ggdb3
	-fno-lto
	-fno-omit-frame-pointer
	-fsanitize=address,undefined
)

# Add here any file that you want to be part of the library target,
# but it is not in a projectFolder/src/dir directory
set(test_sources_list
    # "anotherFolder/myfile.cpp"
    # If none, use an empty string.
    ""
)

# Get all the .cpp files in any projectFolder/src/dir directory
file(GLOB directories */)
foreach(dir ${directories})
    if(IS_DIRECTORY ${dir})
        string(FIND ${dir} "/" last_slash_pos REVERSE)
        math(EXPR string_start "${last_slash_pos}+1")
        string(SUBSTRING ${dir} ${string_start} -1 dir_stripped)
        file(GLOB_RECURSE sources ${dir_stripped}/*.cpp)
        list(APPEND test_sources_list ${sources})
    endif()
endforeach()

add_executable(
	${PROJECT_NAME}_tests
	${test_sources_list}
)

target_link_libraries(
	${PROJECT_NAME}_tests
	PRIVATE
		${PROJECT_NAME}_LIB
		${OpenCV_LIBS}
		GTest::gtest_main
)

# Inside the tests executable, each gtest cpp file can only include:
# (1) standard library headers
# (2) third-party library headers
# (3) project header files via #include "demo_002/..."
# (4) test utility headers without specifying a directory.
target_include_directories(
    ${PROJECT_NAME}_tests
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/tests/test_utils
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)

set_target_properties(
	${PROJECT_NAME}_tests
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
