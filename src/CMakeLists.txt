# This CMakeLists.txt is adapted and minimized from the following:
#    https://github.com/mortinger91/cpp-cmake-template/blob/master/projectFolder/src/CMakeLists.txt
# 

# Add here any file that you want to be part of the library target,
# but it is not in a projectFolder/src/dir directory
set(sources_list
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
        list(APPEND sources_list ${sources})
    endif()
endforeach()

# Create a library target that contains all of the sources
add_library(
    ${PROJECT_NAME}_LIB 
    ${sources_list}
)

# Add the include directories to the library target.
# Remember to add any include that is not in a projectFolder/src/dir directory
target_include_directories(
    ${PROJECT_NAME}_LIB
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

# Define the compiler flags based on the build type
set(COMPILER_FLAGS
    -std=c++17
    -march=native
    -mtune=native
)
list(APPEND COMPILER_FLAGS
    -Wall
    -Wno-unused-variable
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-label
    -Wno-unused-value
    -Wno-unused-but-set-variable
    -Wno-unused-local-typedefs
    -Wno-unused-result
    -Wno-missing-field-initializers
    -Wno-missing-braces
    -Wno-missing-field-initializers
    -Werror=return-type # Fail if a non-void function has a normal exit path that does not return a value
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND COMPILER_FLAGS
        -Og
        -ggdb3
        -fno-lto
        -fno-omit-frame-pointer
        # -fsanitize=address,undefined
    )
    # set(COMPILER_FLAGS
    #     -Wall
    #     -O0
    # )
    # if(UNIX)
    #     list(APPEND COMPILER_FLAGS
    #         -Werror
    #         -Wpedantic
    #         -Wextra
    #     )
    # endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND COMPILER_FLAGS
        -O3
        -g
        -flto
        -fPIC
    )
    # set(COMPILER_FLAGS
    #     -DNDEBUG
    # )
    # if(UNIX)
    #     list(APPEND COMPILER_FLAGS
    #         -O3
    #         -march=native
    #         -ffast-math
    #     )
    # endif()
endif()
# Set the compiler flags for the library target
target_compile_options(
    ${PROJECT_NAME}_LIB
    PUBLIC
        ${COMPILER_FLAGS}
)

# Define the preprocessor macros based on the build type.
# This is equivalent to using #define in the source code
# or using -D flags as a compiler option
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PREPROCESSOR_MACROS
        DEBUG
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PREPROCESSOR_MACROS
        RELEASE
    )
endif()
if(LINUX)
    list(APPEND PREPROCESSOR_MACROS
        LINUX
    )
elseif(APPLE)
    list(APPEND PREPROCESSOR_MACROS
        MACOS
    )
elseif(WIN32)
    list(APPEND PREPROCESSOR_MACROS
        WINDOWS
    )
endif()
# Set the preprocessor macros for the library target
target_compile_definitions(
    ${PROJECT_NAME}_LIB
    PUBLIC
        ${PREPROCESSOR_MACROS}
)

# Set the C++ language features for the library target
target_compile_features(
    ${PROJECT_NAME}_LIB
    PUBLIC
        # c_std_11
        cxx_std_17
)
# Ensure that the C++ standard is mandatory
# and disable compiler-specific extensions
set_target_properties(
    ${PROJECT_NAME}_LIB
        PROPERTIES
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
)

###
# Define the linker options based on the build type
###
# Common linker options for all LINUX AMD64 build types
set(LINKER_OPTIONS
    -pthread
    -latomic
)
###
# Additional linker options for LINUX AMD64 DEBUG
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND LINKER_OPTIONS
        -ggdb3
        -fno-lto
    )
    # set(LINKER_OPTIONS
    # )
###
# Additional linker options for LINUX AMD64 RELEASE
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND LINKER_OPTIONS
        -g
        -flto
        -fPIC
    )
    # set(LINKER_OPTIONS
    # )
    # if(UNIX)
    #     list(APPEND LINKER_OPTIONS
    #         -flto           # Enable link-time optimizations
    #     )
    # endif()
    # if(LINUX)
    #     list(APPEND LINKER_OPTIONS
    #         -Wl,--strip-all # Strip debug symbols
    #     )
    # endif()
endif()
###
# Additional "trailer" linker options
if(LINUX)
    list(APPEND LINKER_OPTIONS
        -Wl,--no-undefined # Fail if there are any unresolved symbols in the final linked output
        -Werror=return-type # Fail if a non-void function has a normal exit path that does not return a value
    )
endif()

# Set the linker options for the library target
target_link_options(
    ${PROJECT_NAME}_LIB
    PUBLIC
        ${LINKER_OPTIONS}
)

target_link_libraries(
    ${PROJECT_NAME}_LIB
    PUBLIC
        # Add libraries to link to the binary here
        ${OpenCV_LIBS}
)
