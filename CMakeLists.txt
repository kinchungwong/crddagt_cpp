cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

project(
    "crddagt_cpp"
    DESCRIPTION "crddagt_cpp (currently part of the repo_20251205 monorepo pending a split)"
    HOMEPAGE_URL "https://github.com/kinchungwong/repo_20251205"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
endif()

include(GNUInstallDirs)

### GoogleTest (local checkout under ../googletest/download)
set(GTEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../googletest/download")
if(EXISTS "${GTEST_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory(${GTEST_SOURCE_DIR} ${CMAKE_BINARY_DIR}/googletest-build EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "GoogleTest source not found at ${GTEST_SOURCE_DIR}")
endif()

### OpenCV
find_package(
    OpenCV 
    REQUIRED 
    PATHS "../opencv-mini/build/out" 
    NO_DEFAULT_PATH
)

add_executable(${PROJECT_NAME} src/main.cpp)
add_subdirectory(src)
target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
        ${OpenCV_LIBS}
    PRIVATE
        ${PROJECT_NAME}_LIB
)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
