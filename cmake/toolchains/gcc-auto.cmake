# Prefer GCC 14 when available, fall back to GCC 13, then unversioned gcc/g++.
# Allows override via environment variables CC/CXX.

if(DEFINED ENV{CC} AND NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "$ENV{CC}" CACHE FILEPATH "" FORCE)
endif()

if(DEFINED ENV{CXX} AND NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE FILEPATH "" FORCE)
endif()

if(NOT DEFINED CMAKE_C_COMPILER)
  find_program(_tg_gcc NAMES gcc-14 gcc-13 gcc)
  if(_tg_gcc)
    set(CMAKE_C_COMPILER "${_tg_gcc}" CACHE FILEPATH "" FORCE)
  endif()
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
  find_program(_tg_gxx NAMES g++-14 g++-13 g++)
  if(_tg_gxx)
    set(CMAKE_CXX_COMPILER "${_tg_gxx}" CACHE FILEPATH "" FORCE)
  endif()
endif()
